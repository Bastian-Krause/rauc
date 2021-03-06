#include <stdio.h>
#include <locale.h>
#include <glib.h>

#include <config_file.h>
#include <context.h>

#include "common.h"
#include "utils.h"

typedef struct {
	gchar *tmpdir;
} ConfigFileFixture;

static void config_file_fixture_set_up(ConfigFileFixture *fixture,
		gconstpointer user_data)
{
	fixture->tmpdir = g_dir_make_tmp("rauc-conf_file-XXXXXX", NULL);
	g_assert_nonnull(fixture->tmpdir);
}

static void config_file_fixture_tear_down(ConfigFileFixture *fixture,
		gconstpointer user_data)
{
	g_assert_true(rm_tree(fixture->tmpdir, NULL));
	g_free(fixture->tmpdir);
}

/* Test: Parse entire config file and check if derived slot / file structures
 * are initialized correctly */
static void config_file_full_config(ConfigFileFixture *fixture,
		gconstpointer user_data)
{
	GList *slotlist;
	RaucConfig *config;
	RaucSlot *slot;


	const gchar *cfg_file = "\
[system]\n\
compatible=FooCorp Super BarBazzer\n\
bootloader=barebox\n\
mountprefix=/mnt/myrauc/\n\
\n\
[keyring]\n\
path=/etc/rauc/keyring/\n\
\n\
[slot.rescue.0]\n\
description=Rescue partition\n\
device=/dev/rescue-0\n\
type=raw\n\
bootname=factory0\n\
readonly=true\n\
\n\
[slot.rootfs.0]\n\
description=Root filesystem partition 0\n\
device=/dev/rootfs-0\n\
type=ext4\n\
bootname=system0\n\
readonly=false\n\
ignore-checksum=false\n\
\n\
[slot.rootfs.1]\n\
description=Root filesystem partition 1\n\
device=/dev/rootfs-1\n\
type=ext4\n\
bootname=system1\n\
readonly=false\n\
ignore-checksum=false\n\
\n\
[slot.appfs.0]\n\
description=Application filesystem partition 0\n\
device=/dev/appfs-0\n\
type=ext4\n\
parent=rootfs.0\n\
ignore-checksum=true\n\
\n\
[slot.appfs.1]\n\
description=Application filesystem partition 1\n\
device=/dev/appfs-1\n\
type=ext4\n\
parent=rootfs.1\n\
ignore-checksum=true\n";

	gchar* pathname = write_tmp_file(fixture->tmpdir, "full_config.conf", cfg_file, NULL);
	g_assert_nonnull(pathname);

	g_assert_true(load_config(pathname, &config, NULL));
	g_assert_nonnull(config);
	g_assert_cmpstr(config->system_compatible, ==, "FooCorp Super BarBazzer");
	g_assert_cmpstr(config->system_bootloader, ==, "barebox");
	g_assert_cmpstr(config->mount_prefix, ==, "/mnt/myrauc/");
	g_assert_true(config->activate_installed);

	g_assert_nonnull(config->slots);
	slotlist = g_hash_table_get_keys(config->slots);

	slot = g_hash_table_lookup(config->slots, "rescue.0");
	g_assert_cmpstr(slot->name, ==, "rescue.0");
	g_assert_cmpstr(slot->description, ==, "Rescue partition");
	g_assert_cmpstr(slot->device, ==, "/dev/rescue-0");
	g_assert_cmpstr(slot->bootname, ==, "factory0");
	g_assert_cmpstr(slot->type, ==, "raw");
	g_assert_true(slot->readonly);
	g_assert_false(slot->ignore_checksum);
	g_assert_null(slot->parent);
	g_assert(find_config_slot_by_device(config, "/dev/rescue-0") == slot);

	slot = g_hash_table_lookup(config->slots, "rootfs.0");
	g_assert_cmpstr(slot->name, ==, "rootfs.0");
	g_assert_cmpstr(slot->description, ==, "Root filesystem partition 0");
	g_assert_cmpstr(slot->device, ==, "/dev/rootfs-0");
	g_assert_cmpstr(slot->bootname, ==, "system0");
	g_assert_cmpstr(slot->type, ==, "ext4");
	g_assert_false(slot->readonly);
	g_assert_false(slot->ignore_checksum);
	g_assert_null(slot->parent);
	g_assert(find_config_slot_by_device(config, "/dev/rootfs-0") == slot);

	slot = g_hash_table_lookup(config->slots, "rootfs.1");
	g_assert_cmpstr(slot->name, ==, "rootfs.1");
	g_assert_cmpstr(slot->description, ==, "Root filesystem partition 1");
	g_assert_cmpstr(slot->device, ==, "/dev/rootfs-1");
	g_assert_cmpstr(slot->bootname, ==, "system1");
	g_assert_cmpstr(slot->type, ==, "ext4");
	g_assert_false(slot->readonly);
	g_assert_false(slot->ignore_checksum);
	g_assert_null(slot->parent);
	g_assert(find_config_slot_by_device(config, "/dev/rootfs-1") == slot);

	slot = g_hash_table_lookup(config->slots, "appfs.0");
	g_assert_cmpstr(slot->name, ==, "appfs.0");
	g_assert_cmpstr(slot->description, ==, "Application filesystem partition 0");
	g_assert_cmpstr(slot->device, ==, "/dev/appfs-0");
	g_assert_null(slot->bootname);
	g_assert_cmpstr(slot->type, ==, "ext4");
	g_assert_false(slot->readonly);
	g_assert_true(slot->ignore_checksum);
	g_assert_nonnull(slot->parent);
	g_assert(find_config_slot_by_device(config, "/dev/appfs-0") == slot);

	slot = g_hash_table_lookup(config->slots, "appfs.1");
	g_assert_cmpstr(slot->name, ==, "appfs.1");
	g_assert_cmpstr(slot->description, ==, "Application filesystem partition 1");
	g_assert_cmpstr(slot->device, ==, "/dev/appfs-1");
	g_assert_null(slot->bootname);
	g_assert_cmpstr(slot->type, ==, "ext4");
	g_assert_false(slot->readonly);
	g_assert_true(slot->ignore_checksum);
	g_assert_nonnull(slot->parent);
	g_assert(find_config_slot_by_device(config, "/dev/appfs-1") == slot);

	g_assert_cmpuint(g_list_length(slotlist), ==, 5);

	g_list_free(slotlist);

	g_assert(find_config_slot_by_device(config, "/dev/xxx0") == NULL);

	free_config(config);
}

static void config_file_bootloaders(ConfigFileFixture *fixture,
		gconstpointer user_data)
{
	RaucConfig *config;
	GError *ierror = NULL;
	gchar* pathname;

	const gchar *boot_inval_cfg_file = "\
[system]\n\
compatible=FooCorp Super BarBazzer\n\
bootloader=superloader2000\n\
mountprefix=/mnt/myrauc/\n";
	const gchar *boot_missing_cfg_file = "\
[system]\n\
compatible=FooCorp Super BarBazzer\n\
mountprefix=/mnt/myrauc/\n";


	pathname = write_tmp_file(fixture->tmpdir, "invalid_bootloader.conf", boot_inval_cfg_file, NULL);
	g_assert_nonnull(pathname);

	g_assert_false(load_config(pathname, &config, &ierror));
	g_assert_cmpstr(ierror->message, ==, "Unsupported bootloader 'superloader2000' selected in system config");
	g_clear_error(&ierror);


	pathname = write_tmp_file(fixture->tmpdir, "invalid_bootloader.conf", boot_missing_cfg_file, NULL);
	g_assert_nonnull(pathname);

	g_assert_false(load_config(pathname, &config, &ierror));
	g_assert_cmpstr(ierror->message, ==, "No bootloader selected in system config");
	g_clear_error(&ierror);
}

static void config_file_invalid_parent(ConfigFileFixture *fixture,
		gconstpointer user_data)
{
	RaucConfig *config;
	GError *ierror = NULL;
	gchar* pathname;

	const gchar *nonexisting_parent = "\
[system]\n\
compatible=FooCorp Super BarBazzer\n\
bootloader=barebox\n\
\n\
[slot.child.0]\n\
device=/dev/null\n\
parent=invalid\n\
	";


	pathname = write_tmp_file(fixture->tmpdir, "nonexisting_bootloader.conf", nonexisting_parent, NULL);
	g_assert_nonnull(pathname);

	g_assert_false(load_config(pathname, &config, &ierror));
	g_assert_error(ierror, R_CONFIG_ERROR, R_CONFIG_ERROR_PARENT);
	g_assert_cmpstr(ierror->message, ==, "Parent slot 'invalid' not found!");
	g_clear_error(&ierror);
}

static void config_file_typo_in_boolean_readonly_key(ConfigFileFixture *fixture,
		gconstpointer user_data)
{
	RaucConfig *config;
	GError *ierror = NULL;
	gchar* pathname;

	const gchar *cfg_file = "\
[system]\n\
compatible=FooCorp Super BarBazzer\n\
bootloader=barebox\n\
mountprefix=/mnt/myrauc/\n\
\n\
[slot.rescue.0]\n\
description=Rescue partition\n\
device=/dev/mtd4\n\
type=raw\n\
bootname=factory0\n\
readonly=typo\n";


	pathname = write_tmp_file(fixture->tmpdir, "invalid_bootloader.conf", cfg_file, NULL);
	g_assert_nonnull(pathname);

	g_assert_false(load_config(pathname, &config, &ierror));
	g_assert_error(ierror, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_INVALID_VALUE);
	g_clear_error(&ierror);
}

static void config_file_typo_in_boolean_ignore_checksum_key(ConfigFileFixture *fixture,
		gconstpointer user_data)
{
	RaucConfig *config;
	GError *ierror = NULL;
	gchar* pathname;

	const gchar *cfg_file = "\
[system]\n\
compatible=FooCorp Super BarBazzer\n\
bootloader=barebox\n\
mountprefix=/mnt/myrauc/\n\
\n\
[slot.rescue.0]\n\
description=Rescue partition\n\
device=/dev/mtd4\n\
type=raw\n\
bootname=factory0\n\
ignore-checksum=typo\n";


	pathname = write_tmp_file(fixture->tmpdir, "invalid_bootloader.conf", cfg_file, NULL);
	g_assert_nonnull(pathname);

	g_assert_false(load_config(pathname, &config, &ierror));
	g_assert_error(ierror, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_INVALID_VALUE);
	g_clear_error(&ierror);
}

static void config_file_typo_in_boolean_activate_installed_key(ConfigFileFixture *fixture,
		gconstpointer user_data)
{
	RaucConfig *config;
	GError *ierror = NULL;
	gchar* pathname;

	const gchar *cfg_file = "\
[system]\n\
compatible=FooCorp Super BarBazzer\n\
bootloader=barebox\n\
mountprefix=/mnt/myrauc/\n\
activate-installed=typo\n";


	pathname = write_tmp_file(fixture->tmpdir, "invalid_bootloader.conf", cfg_file, NULL);
	g_assert_nonnull(pathname);

	g_assert_false(load_config(pathname, &config, &ierror));
	g_assert_error(ierror, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_INVALID_VALUE);
	g_assert_null(config);
	g_clear_error(&ierror);
}

static void config_file_activate_installed_set_to_true(ConfigFileFixture *fixture,
		gconstpointer user_data)
{
	RaucConfig *config;
	GError *ierror = NULL;
	gchar* pathname;

	const gchar *cfg_file = "\
[system]\n\
compatible=FooCorp Super BarBazzer\n\
bootloader=barebox\n\
mountprefix=/mnt/myrauc/\n\
activate-installed=true\n";


	pathname = write_tmp_file(fixture->tmpdir, "invalid_bootloader.conf", cfg_file, NULL);
	g_assert_nonnull(pathname);

	g_assert_true(load_config(pathname, &config, &ierror));
	g_assert_null(ierror);
	g_assert_nonnull(config);
	g_assert_true(config->activate_installed);

	free_config(config);
}

static void config_file_activate_installed_set_to_false(ConfigFileFixture *fixture,
		gconstpointer user_data)
{
	RaucConfig *config;
	GError *ierror = NULL;
	gchar* pathname;

	const gchar *cfg_file = "\
[system]\n\
compatible=FooCorp Super BarBazzer\n\
bootloader=barebox\n\
mountprefix=/mnt/myrauc/\n\
activate-installed=false\n";


	pathname = write_tmp_file(fixture->tmpdir, "invalid_bootloader.conf", cfg_file, NULL);
	g_assert_nonnull(pathname);

	g_assert_true(load_config(pathname, &config, &ierror));
	g_assert_null(ierror);
	g_assert_nonnull(config);
	g_assert_false(config->activate_installed);

	free_config(config);
}

static void config_file_system_variant(ConfigFileFixture *fixture,
		gconstpointer user_data)
{
	RaucConfig *config;
	GError *ierror = NULL;
	gchar* pathname;

	const gchar *cfg_file_no_variant= "\
[system]\n\
compatible=FooCorp Super BarBazzer\n\
bootloader=barebox\n\
mountprefix=/mnt/myrauc/";

	const gchar *cfg_file_name_variant= "\
[system]\n\
compatible=FooCorp Super BarBazzer\n\
bootloader=barebox\n\
mountprefix=/mnt/myrauc/\n\
variant-name=variant-name";

	const gchar *cfg_file_dtb_variant= "\
[system]\n\
compatible=FooCorp Super BarBazzer\n\
bootloader=barebox\n\
mountprefix=/mnt/myrauc/\n\
variant-dtb=true";

	const gchar *cfg_file_file_variant= "\
[system]\n\
compatible=FooCorp Super BarBazzer\n\
bootloader=barebox\n\
mountprefix=/mnt/myrauc/\n\
variant-file=/path/to/file";

	const gchar *cfg_file_conflicting_variants= "\
[system]\n\
compatible=FooCorp Super BarBazzer\n\
bootloader=barebox\n\
mountprefix=/mnt/myrauc/\n\
variant-dtb=true\n\
variant-name=";

	pathname = write_tmp_file(fixture->tmpdir, "no_variant.conf", cfg_file_no_variant, NULL);
	g_assert_nonnull(pathname);

	g_assert_true(load_config(pathname, &config, &ierror));
	g_free(pathname);
	g_assert_null(ierror);
	g_assert_nonnull(config);
	g_assert_null(config->system_variant);

	free_config(config);

	pathname = write_tmp_file(fixture->tmpdir, "name_variant.conf", cfg_file_name_variant, NULL);
	g_assert_nonnull(pathname);

	g_assert_true(load_config(pathname, &config, &ierror));
	g_free(pathname);
	g_assert_null(ierror);
	g_assert_nonnull(config);
	g_assert(config->system_variant_type == R_CONFIG_SYS_VARIANT_NAME);
	g_assert_cmpstr(config->system_variant, ==, "variant-name");

	free_config(config);

	pathname = write_tmp_file(fixture->tmpdir, "dtb_variant.conf", cfg_file_dtb_variant, NULL);
	g_assert_nonnull(pathname);

	g_assert_true(load_config(pathname, &config, &ierror));
	g_free(pathname);
	g_assert_null(ierror);
	g_assert_nonnull(config);
	g_assert(config->system_variant_type == R_CONFIG_SYS_VARIANT_DTB);
	g_assert_null(config->system_variant);

	free_config(config);

	pathname = write_tmp_file(fixture->tmpdir, "file_variant.conf", cfg_file_file_variant, NULL);
	g_assert_nonnull(pathname);

	g_assert_true(load_config(pathname, &config, &ierror));
	g_free(pathname);
	g_assert_null(ierror);
	g_assert_nonnull(config);
	g_assert(config->system_variant_type == R_CONFIG_SYS_VARIANT_FILE);
	g_assert_cmpstr(config->system_variant, ==, "/path/to/file");

	pathname = write_tmp_file(fixture->tmpdir, "conflict_variant.conf", cfg_file_conflicting_variants, NULL);
	g_assert_nonnull(pathname);

	g_assert_false(load_config(pathname, &config, &ierror));
	g_free(pathname);
	g_assert_error(ierror, R_CONFIG_ERROR, R_CONFIG_ERROR_INVALID_FORMAT);
	g_assert_null(config);
}



static void config_file_test_read_slot_status(void)
{
	RaucSlotStatus *ss;
	g_assert_true(read_slot_status("test/rootfs.raucs", &ss, NULL));
	g_assert_nonnull(ss);
	g_assert_cmpstr(ss->status, ==, "ok");
	g_assert_cmpint(ss->checksum.type, ==, G_CHECKSUM_SHA256);
	g_assert_cmpstr(ss->checksum.digest, ==,
			"e437ab217356ee47cd338be0ffe33a3cb6dc1ce679475ea59ff8a8f7f6242b27");

	free_slot_status(ss);
}


static void config_file_test_write_slot_status(void)
{
	RaucSlotStatus *ss = g_new0(RaucSlotStatus, 1);

	ss->status = g_strdup("ok");
	ss->checksum.type = G_CHECKSUM_SHA256;
	ss->checksum.digest = g_strdup("dc626520dcd53a22f727af3ee42c770e56c97a64fe3adb063799d8ab032fe551");

	write_slot_status("test/savedslot.raucs", ss, NULL);

	free_slot_status(ss);

	read_slot_status("test/savedslot.raucs", &ss, NULL);

	g_assert_nonnull(ss);
	g_assert_cmpstr(ss->status, ==, "ok");
	g_assert_cmpint(ss->checksum.type, ==, G_CHECKSUM_SHA256);
	g_assert_cmpstr(ss->checksum.digest, ==,
			"dc626520dcd53a22f727af3ee42c770e56c97a64fe3adb063799d8ab032fe551");

	free_slot_status(ss);
}

static void config_file_system_serial(void)
{
	g_assert_nonnull(r_context()->system_serial);
	g_assert_cmpstr(r_context()->system_serial, ==, "1234");
}

int main(int argc, char *argv[])
{
	setlocale(LC_ALL, "C");

	g_test_init(&argc, &argv, NULL);

	r_context_conf()->configpath = g_strdup("test/test.conf");
	r_context_conf()->handlerextra = g_strdup("--dummy1 --dummy2");
	r_context();

	g_test_add("/config-file/full-config", ConfigFileFixture, NULL,
		   config_file_fixture_set_up, config_file_full_config,
		   config_file_fixture_tear_down);
	g_test_add("/config-file/bootloaders", ConfigFileFixture, NULL,
		   config_file_fixture_set_up, config_file_bootloaders,
		   config_file_fixture_tear_down);
	g_test_add("/config-file/invalid-parent", ConfigFileFixture, NULL,
		   config_file_fixture_set_up, config_file_invalid_parent,
		   config_file_fixture_tear_down);
	g_test_add("/config-file/typo-in-boolean-readonly-key", ConfigFileFixture, NULL,
		   config_file_fixture_set_up, config_file_typo_in_boolean_readonly_key,
		   config_file_fixture_tear_down);
	g_test_add("/config-file/typo-in-boolean-ignore-checksum-key", ConfigFileFixture, NULL,
		   config_file_fixture_set_up, config_file_typo_in_boolean_ignore_checksum_key,
		   config_file_fixture_tear_down);
	g_test_add("/config-file/typo-in-boolean-activate-installed-key", ConfigFileFixture, NULL,
		   config_file_fixture_set_up, config_file_typo_in_boolean_activate_installed_key,
		   config_file_fixture_tear_down);
	g_test_add("/config-file/activate-installed-key-set-to-true", ConfigFileFixture, NULL,
		   config_file_fixture_set_up, config_file_activate_installed_set_to_true,
		   config_file_fixture_tear_down);
	g_test_add("/config-file/activate-installed-key-set-to-false", ConfigFileFixture, NULL,
		   config_file_fixture_set_up, config_file_activate_installed_set_to_false,
		   config_file_fixture_tear_down);
	g_test_add("/config-file/system-variant", ConfigFileFixture, NULL,
		   config_file_fixture_set_up, config_file_system_variant,
		   config_file_fixture_tear_down);
	g_test_add_func("/config-file/read-slot-status", config_file_test_read_slot_status);
	g_test_add_func("/config-file/write-read-slot-status", config_file_test_write_slot_status);
	g_test_add_func("/config-file/system-serial", config_file_system_serial);

	return g_test_run ();
}
