// SPDX-License-Identifier: GPL-2.0
/*
 * Small bridge for Xiaomi/HyperOS charging framework ports.
 *
 * User space writes the current quick-charge state to:
 *   /sys/kernel/micharge_uevent/quick_charge_type
 *
 * The module then emits a kernel KOBJ_CHANGE uevent carrying:
 *   POWER_SUPPLY_NAME=usb
 *   POWER_SUPPLY_QUICK_CHARGE_TYPE=<0..4>
 *
 * MiuiBatteryServiceImpl listens for POWER_SUPPLY_QUICK_CHARGE_TYPE and then
 * sends miui.intent.action.ACTION_QUICK_CHARGE_TYPE to SystemUI.
 */

#include <linux/device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/string.h>

#define MICHARGE_MAX_USB_TYPE_LEN 31

static DEFINE_MUTEX(micharge_lock);
static struct kobject *micharge_kobj;
static int quick_charge_type;
static int power_max_w;
static int online;
static bool emit_on_same = true;
static char usb_type[MICHARGE_MAX_USB_TYPE_LEN + 1] = "USB";

static void sanitize_usb_type(char *value)
{
	size_t i;

	if (!value)
		return;

	strim(value);
	if (!value[0])
		strscpy(value, "USB", MICHARGE_MAX_USB_TYPE_LEN + 1);

	for (i = 0; value[i]; i++) {
		if (value[i] == ' ' || value[i] == '-')
			value[i] = '_';
	}
}

static int micharge_emit_locked(void)
{
	char qct_buf[40];
	char power_buf[40];
	char online_buf[40];
	char present_buf[40];
	char status_buf[40];
	char type_buf[64];
	char *envp[] = {
		"POWER_SUPPLY_NAME=usb",
		qct_buf,
		power_buf,
		online_buf,
		present_buf,
		status_buf,
		type_buf,
		NULL,
	};
	struct power_supply *psy;
	struct kobject *target;
	int ret;

	snprintf(qct_buf, sizeof(qct_buf), "POWER_SUPPLY_QUICK_CHARGE_TYPE=%d",
		 quick_charge_type);
	snprintf(power_buf, sizeof(power_buf), "POWER_SUPPLY_POWER_MAX=%d",
		 power_max_w);
	snprintf(online_buf, sizeof(online_buf), "POWER_SUPPLY_ONLINE=%d", online);
	snprintf(present_buf, sizeof(present_buf), "POWER_SUPPLY_PRESENT=%d", online);
	snprintf(status_buf, sizeof(status_buf), "POWER_SUPPLY_STATUS=%s",
		 online ? "Charging" : "Discharging");
	snprintf(type_buf, sizeof(type_buf), "POWER_SUPPLY_USB_TYPE=%s", usb_type);

	psy = power_supply_get_by_name("usb");
	if (psy) {
		target = &psy->dev.kobj;
		ret = kobject_uevent_env(target, KOBJ_CHANGE, envp);
		power_supply_put(psy);
	} else {
		target = micharge_kobj;
		ret = kobject_uevent_env(target, KOBJ_CHANGE, envp);
	}

	if (ret)
		pr_warn("micharge_uevent: failed to emit qct=%d ret=%d\n",
			quick_charge_type, ret);
	else
		pr_info("micharge_uevent: emitted qct=%d power=%d online=%d type=%s%s\n",
			quick_charge_type, power_max_w, online, usb_type,
			psy ? " via usb power_supply" : " via helper kobject");

	return ret;
}

static ssize_t quick_charge_type_show(struct kobject *kobj,
				      struct kobj_attribute *attr, char *buf)
{
	ssize_t ret;

	mutex_lock(&micharge_lock);
	ret = sysfs_emit(buf, "%d\n", quick_charge_type);
	mutex_unlock(&micharge_lock);
	return ret;
}

static ssize_t quick_charge_type_store(struct kobject *kobj,
				       struct kobj_attribute *attr,
				       const char *buf, size_t count)
{
	int new_type;
	bool should_emit;
	int ret;

	ret = kstrtoint(buf, 0, &new_type);
	if (ret)
		return ret;

	if (new_type < 0)
		new_type = 0;
	if (new_type > 4)
		new_type = 4;

	mutex_lock(&micharge_lock);
	should_emit = emit_on_same || new_type != quick_charge_type;
	quick_charge_type = new_type;
	if (should_emit)
		micharge_emit_locked();
	mutex_unlock(&micharge_lock);

	return count;
}

static ssize_t power_max_show(struct kobject *kobj, struct kobj_attribute *attr,
			      char *buf)
{
	ssize_t ret;

	mutex_lock(&micharge_lock);
	ret = sysfs_emit(buf, "%d\n", power_max_w);
	mutex_unlock(&micharge_lock);
	return ret;
}

static ssize_t power_max_store(struct kobject *kobj, struct kobj_attribute *attr,
			       const char *buf, size_t count)
{
	int value;
	int ret;

	ret = kstrtoint(buf, 0, &value);
	if (ret)
		return ret;

	mutex_lock(&micharge_lock);
	power_max_w = max(value, 0);
	mutex_unlock(&micharge_lock);
	return count;
}

static ssize_t online_show(struct kobject *kobj, struct kobj_attribute *attr,
			   char *buf)
{
	ssize_t ret;

	mutex_lock(&micharge_lock);
	ret = sysfs_emit(buf, "%d\n", online);
	mutex_unlock(&micharge_lock);
	return ret;
}

static ssize_t online_store(struct kobject *kobj, struct kobj_attribute *attr,
			    const char *buf, size_t count)
{
	bool value;
	int ret;

	ret = kstrtobool(buf, &value);
	if (ret)
		return ret;

	mutex_lock(&micharge_lock);
	online = value ? 1 : 0;
	mutex_unlock(&micharge_lock);
	return count;
}

static ssize_t usb_type_show(struct kobject *kobj, struct kobj_attribute *attr,
			     char *buf)
{
	ssize_t ret;

	mutex_lock(&micharge_lock);
	ret = sysfs_emit(buf, "%s\n", usb_type);
	mutex_unlock(&micharge_lock);
	return ret;
}

static ssize_t usb_type_store(struct kobject *kobj, struct kobj_attribute *attr,
			      const char *buf, size_t count)
{
	char value[MICHARGE_MAX_USB_TYPE_LEN + 1];

	strscpy(value, buf, sizeof(value));
	sanitize_usb_type(value);

	mutex_lock(&micharge_lock);
	strscpy(usb_type, value, sizeof(usb_type));
	mutex_unlock(&micharge_lock);
	return count;
}

static ssize_t emit_on_same_show(struct kobject *kobj,
				 struct kobj_attribute *attr, char *buf)
{
	ssize_t ret;

	mutex_lock(&micharge_lock);
	ret = sysfs_emit(buf, "%d\n", emit_on_same ? 1 : 0);
	mutex_unlock(&micharge_lock);
	return ret;
}

static ssize_t emit_on_same_store(struct kobject *kobj,
				  struct kobj_attribute *attr,
				  const char *buf, size_t count)
{
	bool value;
	int ret;

	ret = kstrtobool(buf, &value);
	if (ret)
		return ret;

	mutex_lock(&micharge_lock);
	emit_on_same = value;
	mutex_unlock(&micharge_lock);
	return count;
}

static ssize_t emit_store(struct kobject *kobj, struct kobj_attribute *attr,
			  const char *buf, size_t count)
{
	mutex_lock(&micharge_lock);
	micharge_emit_locked();
	mutex_unlock(&micharge_lock);
	return count;
}

static ssize_t status_show(struct kobject *kobj, struct kobj_attribute *attr,
			   char *buf)
{
	ssize_t ret;

	mutex_lock(&micharge_lock);
	ret = sysfs_emit(buf,
			 "quick_charge_type=%d\npower_max=%d\nonline=%d\nusb_type=%s\nemit_on_same=%d\n",
			 quick_charge_type, power_max_w, online, usb_type,
			 emit_on_same ? 1 : 0);
	mutex_unlock(&micharge_lock);
	return ret;
}

static struct kobj_attribute quick_charge_type_attr =
	__ATTR_RW(quick_charge_type);
static struct kobj_attribute power_max_attr = __ATTR_RW(power_max);
static struct kobj_attribute online_attr = __ATTR_RW(online);
static struct kobj_attribute usb_type_attr = __ATTR_RW(usb_type);
static struct kobj_attribute emit_on_same_attr = __ATTR_RW(emit_on_same);
static struct kobj_attribute emit_attr = __ATTR_WO(emit);
static struct kobj_attribute status_attr = __ATTR_RO(status);

static struct attribute *micharge_attrs[] = {
	&quick_charge_type_attr.attr,
	&power_max_attr.attr,
	&online_attr.attr,
	&usb_type_attr.attr,
	&emit_on_same_attr.attr,
	&emit_attr.attr,
	&status_attr.attr,
	NULL,
};

static const struct attribute_group micharge_attr_group = {
	.attrs = micharge_attrs,
};

static int __init micharge_uevent_init(void)
{
	int ret;

	micharge_kobj = kobject_create_and_add("micharge_uevent", kernel_kobj);
	if (!micharge_kobj)
		return -ENOMEM;

	ret = sysfs_create_group(micharge_kobj, &micharge_attr_group);
	if (ret) {
		kobject_put(micharge_kobj);
		micharge_kobj = NULL;
		return ret;
	}

	pr_info("micharge_uevent: loaded\n");
	return 0;
}

static void __exit micharge_uevent_exit(void)
{
	if (micharge_kobj) {
		sysfs_remove_group(micharge_kobj, &micharge_attr_group);
		kobject_put(micharge_kobj);
		micharge_kobj = NULL;
	}

	pr_info("micharge_uevent: unloaded\n");
}

module_init(micharge_uevent_init);
module_exit(micharge_uevent_exit);

MODULE_DESCRIPTION("Xiaomi micharge quick-charge power_supply uevent helper");
MODULE_LICENSE("GPL");
