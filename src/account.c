/*
  Vita Account Manager - Switch between multiple PSN/SEN accounts on a PS Vita or PS TV.
  Copyright (C) 2019  "windsurfer1122"
  https://github.com/windsurfer1122

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <stdlib.h>  // for malloc(), free()
#include <vitasdk.h>

#include <account.h>
#include <common.h>
#include <dir.h>
#include <file.h>
#include <history.h>
#include <main.h>
#include <registry.h>

#include <debugScreen.h>
#define printf psvDebugScreenPrintf

const char *const accounts_folder = "accounts/";

const char *const reg_config_np = "/CONFIG/NP";
const char *const reg_config_system = "/CONFIG/SYSTEM";
const char *const file_reg_config_np = "registry/CONFIG/NP/";
const char *const file_reg_config_system = "registry/CONFIG/SYSTEM/";
const int reg_id_username = 14;
const int reg_id_login_id = 258;
const int reg_id_lang = 271;
const int reg_id_country = 270;
const int reg_id_yob = 272;
const int reg_id_mob = 273;
const int reg_id_dob = 274;
const int reg_id_env = 260;

// values from os0:kd/registry.db0 and https://github.com/devnoname120/RegistryEditorMOD/blob/master/regs.c
struct Registry_Entry template_account_reg_entries[] = {
	{ reg_id_username, reg_config_system, NULL, NULL, "username", KEY_TYPE_STR, 17, NULL, },
	{ reg_id_login_id, reg_config_np, file_reg_config_np, NULL, "login_id", KEY_TYPE_STR, 65, NULL, },
	{ 257, reg_config_np, file_reg_config_np, NULL, "account_id", KEY_TYPE_BIN, 8, NULL, },
	{ 259, reg_config_np, file_reg_config_np, NULL, "password", KEY_TYPE_STR, 31, NULL, },
	{ reg_id_country, reg_config_np, file_reg_config_np, NULL, "country", KEY_TYPE_STR, 3, NULL, },
	{ reg_id_lang, reg_config_np, file_reg_config_np, NULL, "lang", KEY_TYPE_STR, 6, NULL, },
	{ reg_id_dob, reg_config_np, file_reg_config_np, NULL, "dob", KEY_TYPE_INT, 4, NULL, },
	{ reg_id_mob, reg_config_np, file_reg_config_np, NULL, "mob", KEY_TYPE_INT, 4, NULL, },
	{ reg_id_yob, reg_config_np, file_reg_config_np, NULL, "yob", KEY_TYPE_INT, 4, NULL, },
	{ reg_id_env, reg_config_np, file_reg_config_np, NULL, "env", KEY_TYPE_STR, 17, NULL, },
	{ 275, reg_config_np, file_reg_config_np, NULL, "has_subaccount", KEY_TYPE_INT, 4, NULL, },
	{ 269, reg_config_np, file_reg_config_np, NULL, "download_confirmed", KEY_TYPE_INT, 4, NULL, },
	{ 256, reg_config_np, file_reg_config_np, NULL, "enable_np", KEY_TYPE_INT, 4, NULL, },
};

struct Registry_Data template_account_reg_data = {
	.reg_count = sizeof(template_account_reg_entries) / sizeof(template_account_reg_entries[0]),
	.reg_size = sizeof(template_account_reg_entries),
	.reg_entries = template_account_reg_entries,
	.idx_username = -1,
	.idx_login_id = -1,
	.idx_ssid = -1,
	.idx_conf_name = -1,
};

struct File_Entry template_account_file_entries[] = {
	{ "tm0:", "tm0/", "npdrm/act.dat", 0, },
	{ "tm0:", "tm0/", "psmdrm/act.dat", 0, },
	{ "ur0:user/00/np/", "ur0/np/", "myprofile.dat", 0, },
	{ "ur0:user/00/trophy/data/sce_trop/", NULL, "TRPUSER.DAT", 0, },
	{ "ur0:user/00/trophy/data/sce_trop/sce_pfs/", NULL, "files.db", 0, },
};

struct File_Data template_account_file_data = {
	.file_count = sizeof(template_account_file_entries) / sizeof(template_account_file_entries[0]),
	.file_size = sizeof(template_account_file_entries),
	.file_entries = template_account_file_entries,
};

struct File_Entry account_unlink_file_entries[] = {
	{ "ux0:", NULL, "id.dat", 0, },
	{ "imc0:", NULL, "id.dat", 0, },
	{ "uma0:", NULL, "id.dat", 0, },
};

struct File_Data account_unlink_file_data = {
	.file_count = sizeof(account_unlink_file_entries) / sizeof(account_unlink_file_entries[0]),
	.file_size = sizeof(account_unlink_file_entries),
	.file_entries = account_unlink_file_entries,
};

void init_account_reg_data(struct Registry_Data **reg_data_ptr)
{
	init_reg_data(reg_data_ptr, &template_account_reg_data);

	return;
}

void get_initial_account_reg_data(struct Registry_Data *reg_data)
{
	int i;
	int key_id;
	unsigned int user_number;
	char string[(STRING_BUFFER_DEFAULT_SIZE)+1];
	char *value;

	string[(STRING_BUFFER_DEFAULT_SIZE)] = '\0';

	// set/get initial account registry data
	for (i = 0; i < reg_data->reg_count; i++) {
		key_id = reg_data->reg_entries[i].key_id;
		if (key_id == reg_id_username) {  // create dummy user name
			sceKernelGetRandomNumber((void *)(&user_number), sizeof(user_number));
			user_number %= 998;
			user_number++;
			sceClibSnprintf(string, (STRING_BUFFER_DEFAULT_SIZE), "user%03i", user_number);
			//
			value = (char *)(reg_data->reg_entries[i].key_value);
			sceClibStrncpy(value, string, reg_data->reg_entries[i].key_size);
			value[reg_data->reg_entries[i].key_size] = '\0';
		} else if (key_id == reg_id_lang) {  // keep language
			value = (char *)(reg_data->reg_entries[i].key_value);
			sceRegMgrGetKeyStr(reg_data->reg_entries[i].key_path, reg_data->reg_entries[i].key_name, value, reg_data->reg_entries[i].key_size);
			if (value[0] == '\0') {  // fallback dummy
				sceClibStrncpy(value, "en", reg_data->reg_entries[i].key_size);
			}
			value[reg_data->reg_entries[i].key_size] = '\0';
		} else if (key_id == reg_id_country) {  // keep country
			value = (char *)(reg_data->reg_entries[i].key_value);
			sceRegMgrGetKeyStr(reg_data->reg_entries[i].key_path, reg_data->reg_entries[i].key_name, value, reg_data->reg_entries[i].key_size);
			if (value[0] == '\0') {  // fallback dummy
				sceClibStrncpy(value, "gb", reg_data->reg_entries[i].key_size);
			}
			value[reg_data->reg_entries[i].key_size] = '\0';
		} else if (key_id == reg_id_yob) {  // dummy year
			*((int *)(reg_data->reg_entries[i].key_value)) = 2000;
		} else if (key_id == reg_id_mob) {  // dummy month
			*((int *)(reg_data->reg_entries[i].key_value)) = 1;
		} else if (key_id == reg_id_dob) {  // dummy day
			*((int *)(reg_data->reg_entries[i].key_value)) = 1;
		} else if (key_id == reg_id_env) {  // keep environment
			value = (char *)(reg_data->reg_entries[i].key_value);
			sceRegMgrGetKeyStr(reg_data->reg_entries[i].key_path, reg_data->reg_entries[i].key_name, value, reg_data->reg_entries[i].key_size);
			if (value[0] == '\0') {  // fallback dummy
				sceClibStrncpy(value, "np", reg_data->reg_entries[i].key_size);
			}
			value[reg_data->reg_entries[i].key_size] = '\0';
		}
	}

	return;
}

void get_current_account_reg_data(struct Registry_Data *reg_data)
{
	int i;

	for (i = 0; i < reg_data->reg_count; i++) {
		if (reg_data->reg_entries[i].key_value == NULL) {
			continue;
		}
		sceClibMemset(reg_data->reg_entries[i].key_value, 0x00, reg_data->reg_entries[i].key_size);

		switch(reg_data->reg_entries[i].key_type) {
			case KEY_TYPE_INT:
				sceRegMgrGetKeyInt(reg_data->reg_entries[i].key_path, reg_data->reg_entries[i].key_name, (int *)(reg_data->reg_entries[i].key_value));
				break;
			case KEY_TYPE_STR:
				sceRegMgrGetKeyStr(reg_data->reg_entries[i].key_path, reg_data->reg_entries[i].key_name, (char *)(reg_data->reg_entries[i].key_value), reg_data->reg_entries[i].key_size);
				((char *)(reg_data->reg_entries[i].key_value))[reg_data->reg_entries[i].key_size] = '\0';
				break;
			case KEY_TYPE_BIN:
				sceRegMgrGetKeyBin(reg_data->reg_entries[i].key_path, reg_data->reg_entries[i].key_name, reg_data->reg_entries[i].key_value, reg_data->reg_entries[i].key_size);
				break;
		}
	}

	return;
}

void init_account_file_data(struct File_Data *file_data)
{
	// copy account template to new file entries array + unlink entries
	sceClibMemcpy((void *)(file_data), (void *)(&template_account_file_data), sizeof(template_account_file_data));
	file_data->file_count += account_unlink_file_data.file_count;
	file_data->file_size += account_unlink_file_data.file_size;
	//
	file_data->file_entries = (struct File_Entry *)malloc(file_data->file_size);
	sceClibMemcpy((void *)(file_data->file_entries), (void *)(template_account_file_data.file_entries), template_account_file_data.file_size);
	sceClibMemcpy((void *)(&(file_data->file_entries[template_account_file_data.file_count])), (void *)(account_unlink_file_data.file_entries), account_unlink_file_data.file_size);

	return;
}

void get_current_account_file_data(struct File_Data *file_data)
{
	int i;
	char source_path[(MAX_PATH_LENGTH)+1];

	source_path[(MAX_PATH_LENGTH)] = '\0';
	for (i = 0; i < file_data->file_count; i++) {
		if ((file_data->file_entries[i].file_path == NULL) || (file_data->file_entries[i].file_name_path == NULL)) {
			file_data->file_entries[i].file_available = 0;
			continue;
		}

		sceClibStrncpy(source_path, file_data->file_entries[i].file_path, (MAX_PATH_LENGTH));
		sceClibStrncat(source_path, file_data->file_entries[i].file_name_path, (MAX_PATH_LENGTH));
		file_data->file_entries[i].file_available = check_file_exists(source_path);
	}

	return;
}

void set_account_file_data(struct File_Data *file_data, char *username)
{
	int i;
	int size_base_path;
	char source_path[(MAX_PATH_LENGTH)+1];
	char target_path[(MAX_PATH_LENGTH)+1];

	source_path[(MAX_PATH_LENGTH)] = '\0';
	target_path[(MAX_PATH_LENGTH)] = '\0';

	// build source base path
	size_base_path = 0;
	if (username != NULL) {
		sceClibStrncpy(source_path, app_base_path, (MAX_PATH_LENGTH));
		sceClibStrncat(source_path, accounts_folder, (MAX_PATH_LENGTH));
		sceClibStrncat(source_path, username, (MAX_PATH_LENGTH));
		sceClibStrncat(source_path, slash_folder, (MAX_PATH_LENGTH));
		size_base_path = sceClibStrnlen(source_path, (MAX_PATH_LENGTH));
	}

	for (i = 0; i < file_data->file_count; i++) {
		if ((file_data->file_entries[i].file_path == NULL) || (file_data->file_entries[i].file_name_path == NULL)) {
			continue;
		}

		// build target path
		sceClibStrncpy(target_path, file_data->file_entries[i].file_path, (MAX_PATH_LENGTH));
		sceClibStrncat(target_path, file_data->file_entries[i].file_name_path, (MAX_PATH_LENGTH));

		if ((username != NULL) && (file_data->file_entries[i].file_available)) {
			// build source path
			source_path[size_base_path] = '\0';
			sceClibStrncat(source_path, file_data->file_entries[i].file_save_path, (MAX_PATH_LENGTH));
			sceClibStrncat(source_path, file_data->file_entries[i].file_name_path, (MAX_PATH_LENGTH));
			// always remove target
			if (check_file_exists(target_path)) {
				printf("\e[2mDeleting target %s...\e[22m\e[0K\n", target_path);
				sceIoRemove(target_path);
			}
			// copy source
			if (!check_file_exists(source_path)) {
				printf("\e[1mMissing source %s...\e[22m\e[0K\n", &(source_path[size_base_path]));
			} else {
				// create target path directories
				create_path(target_path, 0, 1);
				// copy file
				printf("\e[2mCopying %s...\e[22m\e[0K\n", &(source_path[size_base_path]));
				copy_file(source_path, target_path);
			}
		} else {
			if (!check_file_exists(target_path)) {
				printf("\e[2mSkip deleting missing %s...\e[22m\e[0K\n", target_path);
			} else {
				printf("\e[2mDeleting %s...\e[22m\e[0K\n", target_path);
				sceIoRemove(target_path);
			}
		}
	}

	return;
}

void unlink_all_memory_cards(char *title)
{
	// draw title line
	draw_title_line(title);

	// draw pixel line
	draw_pixel_line(NULL, NULL);

	set_account_file_data(&account_unlink_file_data, NULL);

	printf("All memory cards unlinked!\e[0K\n");
	wait_for_cancel_button();

	return;
}

char* combination_data = NULL;

char* get_user_combination(const char* username) {
	char user_combination_path[(MAX_PATH_LENGTH)+1];

	// build target base path
	user_combination_path[(MAX_PATH_LENGTH)] = '\0';
	sceClibStrncpy(user_combination_path, app_base_path, (MAX_PATH_LENGTH));
	sceClibStrncat(user_combination_path, accounts_folder, (MAX_PATH_LENGTH));
	sceClibStrncat(user_combination_path, slash_folder, (MAX_PATH_LENGTH));
	sceClibStrncat(user_combination_path, "combinations.conf", (MAX_PATH_LENGTH));

	int bytes_read;
    if (!combination_data) {
		combination_data = (char*)malloc(1024 * sizeof(char));
    	bytes_read = read_file(user_combination_path, combination_data, 1024);
	}

    char* combination_data_ptr = combination_data;
	char line[512];
	while (sscanf(combination_data_ptr, "%[^\n]\n", line) == 1) {
		char username_a[256];
		char* user_combination = (char*)malloc(256 * sizeof(char));
		if (sscanf(line, "%s %s", username_a, user_combination) == 2)
			if (sceClibStrcmp(username_a, username) == 0) {
				return user_combination;
			}
		combination_data_ptr += strlen(line) + 1;
	} return NULL;
}

void display_account_details_short(struct Registry_Data *reg_data, int *no_user)
{
	int len;

	if (no_user != NULL) {
		*no_user = 0;
	}
	// username
	printf("Current User Name: ");
	if ((reg_data->idx_username >= 0) && (reg_data->reg_entries[reg_data->idx_username].key_value != NULL) && ((len = sceClibStrnlen((char *)(reg_data->reg_entries[reg_data->idx_username].key_value), reg_data->reg_entries[reg_data->idx_username].key_size)) > 0)) {
		printf("%s\e[0K\n", (char *)(reg_data->reg_entries[reg_data->idx_username].key_value), len);
	} else {
		printf("<None>\e[0K\n");
		if (no_user != NULL) {
			*no_user = 1;
		}
	}
	// login id
	printf("Current Login ID: ");
	if ((reg_data->idx_login_id >= 0) && (reg_data->reg_entries[reg_data->idx_login_id].key_value != NULL) && ((len = sceClibStrnlen((char *)(reg_data->reg_entries[reg_data->idx_login_id].key_value), reg_data->reg_entries[reg_data->idx_login_id].key_size)) > 0)) {
		printf("%s\e[0K\n", (char *)(reg_data->reg_entries[reg_data->idx_login_id].key_value), len);
	} else {
		printf("<None>\e[0K\n");
		if (no_user != NULL) {
			*no_user = 1;
		}
	}

	char* username = (char *)(reg_data->reg_entries[reg_data->idx_username].key_value);
	char* user_combination = get_user_combination(username);
	if (user_combination){
		printf("Current Combination: %s\n", user_combination);
		free(user_combination);
	}

	return;
}

void display_account_details_full(struct Registry_Data *reg_data, struct File_Data *file_data, char *title)
{
	int i, j;
	int len;

	if (title != NULL) {
		// draw title line
		draw_title_line(title);

		// draw pixel line
		draw_pixel_line(NULL, NULL);
	}

	// registry data
	printf("Registry Data:\e[0K\n");
	for (i = 0; i < reg_data->reg_count; i++) {
		printf("\e[2m%s/\e[22m%s: ", reg_data->reg_entries[i].key_path, reg_data->reg_entries[i].key_name);

		switch(reg_data->reg_entries[i].key_type) {
			case KEY_TYPE_INT:
				printf("%i\e[0K\n", *((int *)(reg_data->reg_entries[i].key_value)));
				break;
			case KEY_TYPE_STR:
				if ((reg_data->reg_entries[i].key_value != NULL) && ((len = sceClibStrnlen((char *)(reg_data->reg_entries[i].key_value), reg_data->reg_entries[i].key_size)) > 0)) {
					printf("%s (%i)\e[0K\n", (char *)(reg_data->reg_entries[i].key_value), len);
				} else {
					printf("<None>\e[0K\n");
				}
				break;
			case KEY_TYPE_BIN:
				for (j = 0; j < reg_data->reg_entries[i].key_size; j++) {
					printf("%02x", ((unsigned char *)(reg_data->reg_entries[i].key_value))[j]);
				}
				printf(" (%i)\e[0K\n", reg_data->reg_entries[i].key_size);
				break;
		}
	}

	// file data
	printf("File Data:\e[0K\n");
	for (i = 0; i < file_data->file_count; i++) {
		if ((file_data->file_entries[i].file_path == NULL) || (file_data->file_entries[i].file_name_path == NULL)) {
			continue;
		}

		printf("\e[2m%s\e[22m%s: ", file_data->file_entries[i].file_path, file_data->file_entries[i].file_name_path);

		if (file_data->file_entries[i].file_available) {
			printf("available\e[0K\n");
		} else {
			printf("missing\e[0K\n");
		}
	}

	if (title != NULL) {
		wait_for_cancel_button();
	}

	return;
}

void save_account_details(struct Registry_Data *reg_data, struct File_Data *file_data, char *title)
{
	int i;
	int size_base_path;
	char base_path[(MAX_PATH_LENGTH)+1];
	char target_path[(MAX_PATH_LENGTH)+1];
	char source_path[(MAX_PATH_LENGTH)+1];

	// draw title line
	draw_title_line(title);

	// draw pixel line
	draw_pixel_line(NULL, NULL);

	// check username and login id
	if ((reg_data->idx_username < 0) || (reg_data->reg_entries[reg_data->idx_username].key_value == NULL) || (sceClibStrnlen((char *)(reg_data->reg_entries[reg_data->idx_username].key_value), 1) == 0)  // check username
	    || (reg_data->idx_login_id < 0) || (reg_data->reg_entries[reg_data->idx_login_id].key_value == NULL) || (sceClibStrnlen((char *)(reg_data->reg_entries[reg_data->idx_login_id].key_value), 1) == 0))  // check login id
	{
		printf("\e[1mThere is no linked account.\e[22m\e[0K\n");
		sceKernelDelayThread(1500000);  // 1.5s
		return;
	}

	source_path[(MAX_PATH_LENGTH)] = '\0';

	// build target base path
	base_path[(MAX_PATH_LENGTH)] = '\0';
	sceClibStrncpy(base_path, app_base_path, (MAX_PATH_LENGTH));
	sceClibStrncat(base_path, accounts_folder, (MAX_PATH_LENGTH));
	sceClibStrncat(base_path, (char *)(reg_data->reg_entries[reg_data->idx_username].key_value), (MAX_PATH_LENGTH));
	sceClibStrncat(base_path, slash_folder, (MAX_PATH_LENGTH));
	printf("Saving account details to %s...\e[0K\n", base_path);

	// save account registry data
	save_reg_data(base_path, reg_data);

	// save account file data
	size_base_path = sceClibStrnlen(base_path, (MAX_PATH_LENGTH));
	sceClibStrncpy(target_path, base_path, (MAX_PATH_LENGTH));
	target_path[size_base_path] = '\0';
	target_path[(MAX_PATH_LENGTH)] = '\0';
	create_path(target_path, 0, 0);
	for (i = 0; i < file_data->file_count; i++) {
		if ((file_data->file_entries[i].file_save_path == NULL) || (file_data->file_entries[i].file_name_path == NULL) || (file_data->file_entries[i].file_path == NULL)) {
			continue;
		}

		// build source path
		sceClibStrncpy(source_path, file_data->file_entries[i].file_path, (MAX_PATH_LENGTH));
		sceClibStrncat(source_path, file_data->file_entries[i].file_name_path, (MAX_PATH_LENGTH));

		if (!file_data->file_entries[i].file_available) {
			printf("\e[2mSkip missing %s...\e[22m\e[0K\n", source_path);
			continue;
		}

		// build target path
		target_path[size_base_path] = '\0';
		sceClibStrncat(target_path, file_data->file_entries[i].file_save_path, (MAX_PATH_LENGTH));
		sceClibStrncat(target_path, file_data->file_entries[i].file_name_path, (MAX_PATH_LENGTH));

		// create target path directories
		create_path(target_path, size_base_path, 0);

		// copy file
		printf("\e[2mCopying %s...\e[22m\e[0K\n", source_path);
		copy_file(source_path, target_path);
	}

	printf("Account %s saved!\e[0K\n", (char *)(reg_data->reg_entries[reg_data->idx_username].key_value));
	wait_for_cancel_button();

	return;
}

void read_account_details(struct Registry_Data *reg_data, struct File_Data *file_data, struct Registry_Data *reg_init_data, struct File_Data *file_init_data)
{
	int i;
	int size_base_path;
	char base_path[(MAX_PATH_LENGTH)+1];
	char source_path[(MAX_PATH_LENGTH)+1];

	// build source base path
	base_path[(MAX_PATH_LENGTH)] = '\0';
	sceClibStrncpy(base_path, app_base_path, (MAX_PATH_LENGTH));
	sceClibStrncat(base_path, accounts_folder, (MAX_PATH_LENGTH));
	sceClibStrncat(base_path, (char *)(reg_data->reg_entries[reg_data->idx_username].key_value), (MAX_PATH_LENGTH));
	sceClibStrncat(base_path, slash_folder, (MAX_PATH_LENGTH));
	size_base_path = sceClibStrnlen(base_path, (MAX_PATH_LENGTH));
	printf("Reading account details from %s...\e[0K\n", base_path);

	// load account registry data
	load_reg_data(base_path, reg_data, reg_init_data, reg_id_username, -1);

	// load account file data
	sceClibStrncpy(source_path, base_path, (MAX_PATH_LENGTH));
	source_path[size_base_path] = '\0';
	source_path[(MAX_PATH_LENGTH)] = '\0';
	for (i = 0; i < file_data->file_count; i++) {
		if ((file_data->file_entries[i].file_save_path == NULL) || (file_data->file_entries[i].file_path == NULL) || (file_data->file_entries[i].file_name_path == NULL)) {
			file_data->file_entries[i].file_available = 0;
			continue;
		}

		// build source path
		source_path[size_base_path] = '\0';
		sceClibStrncat(source_path, file_data->file_entries[i].file_save_path, (MAX_PATH_LENGTH));
		sceClibStrncat(source_path, file_data->file_entries[i].file_name_path, (MAX_PATH_LENGTH));
		file_data->file_entries[i].file_available = check_file_exists(source_path);
	}
}

int switch_account(struct Registry_Data *reg_data, struct Registry_Data *reg_init_data, struct File_Data *file_init_data, char *title)
{
	int result;
	int menu_redraw_screen;
	int menu_redraw;
	int menu_run;
	int menu_items;
	int menu_item;
	int x, y;
	int x2, y2;
	int x3, y3;
	int button_pressed;
	int i;
	int size_base_path;
	char base_path[(MAX_PATH_LENGTH)+1];
	struct Dir_Entry *dirs;
	int dir_count;
	int dir_count2;
	int entries_per_screen;
	int size;
	int count;

	result = 0;

	// build source base path
	base_path[(MAX_PATH_LENGTH)] = '\0';
	sceClibStrncpy(base_path, app_base_path, (MAX_PATH_LENGTH));
	sceClibStrncat(base_path, accounts_folder, (MAX_PATH_LENGTH));
	size_base_path = sceClibStrnlen(base_path, (MAX_PATH_LENGTH));

	// read directories in base path
	base_path[size_base_path - 1] = '\0';
	dirs = NULL;
	dir_count = get_subdirs(base_path, &dirs);
	base_path[size_base_path - 1] = '/';

	// run switch menu
	menu_redraw_screen = 1;
	menu_redraw = 1;
	menu_run = 1;
	menu_items = 0;
	menu_item = 0;
	dir_count2 = 0;
	count = dir_count2;
	do {
		// redraw screen
		if (menu_redraw_screen) {
			// draw title line
			draw_title_line(title);

			// draw pixel line
			draw_pixel_line(NULL, NULL);
			psvDebugScreenGetCoordsXY(NULL, &y3);  // start of data
			x3 = 0;

			// draw current account data
			display_account_details_short(reg_data, NULL);

			// draw pixel line
			draw_pixel_line(NULL, NULL);

			// draw info
			printf("Switch to another account by changing account data\e[0K\n");
			printf("in registry and replacing account files.\e[0K\n");
			printf("\e[2K\nThe following %i accounts are available: (L/R to page)\e[0K\n", dir_count);

			// draw first part of menu
			psvDebugScreenGetCoordsXY(NULL, &y);  // start of menu
			x = 0;
			printf(" Cancel.\e[0K\n");
			psvDebugScreenGetCoordsXY(NULL, &y2);  // start of account list
			x2 = 0;

			entries_per_screen = ((SCREEN_HEIGHT) - y2 + 1) / psv_font_current->size_h;

			menu_redraw = 1;
			menu_redraw_screen = 0;
		}

		// redraw account list
		if (menu_redraw) {
			psvDebugScreenSetCoordsXY(&x2, &y2);
			printf("\e[0J");

			count = dir_count2;
			menu_items = 0;
			for (i = 0; (i < entries_per_screen) && (count < dir_count); i++, count++) {
				menu_items++;
				size = (dirs[count].size > (reg_init_data->reg_entries[reg_init_data->idx_username].key_size - 1));
				if (size) {
					printf("\e[2m");
				}
				printf(" %.*s", reg_init_data->reg_entries[reg_init_data->idx_username].key_size, dirs[count].name);
				if (size) {
					printf("... (name too long)\e[22m");
				}

				char* username = (char*)dirs[count].name;
				char* user_combination = get_user_combination(username);
				if (user_combination){
					printf("| %s", user_combination);
					free(user_combination);
				}

				printf("\e[0K\n");
			}

			menu_redraw = 0;
		}

		// draw menu marker
		psvDebugScreenSetCoordsXY(&x, &y);
		//
		if (menu_item < 0) {
			menu_item = 0;
		}
		if (menu_item > menu_items) {
			menu_item = menu_items;
		}
		//
		for (i = 0; i <= menu_items; i++) {
			if (menu_item == i) {
				printf(">\n");
			} else {
				printf(" \n");
			}
		}

		// process key strokes
		button_pressed = get_key();
		if (button_pressed == SCE_CTRL_DOWN) {
			menu_item++;
		} else if (button_pressed == SCE_CTRL_UP) {
			menu_item--;
		} else if (button_pressed == SCE_CTRL_RTRIGGER) {
			if (count < dir_count) {
				dir_count2 += entries_per_screen;
				if (dir_count2 >= dir_count) {
					dir_count2 = dir_count - 1;
				}
				menu_redraw = 1;
			}
		} else if (button_pressed == SCE_CTRL_LTRIGGER) {
			if (dir_count2 > 0) {
				dir_count2 -= entries_per_screen;
				if (dir_count2 < 0) {
					dir_count2 = 0;
				}
				menu_redraw = 1;
			}
		} else if (button_pressed == button_enter) {
			if (menu_item == 0) {  // cancel
				menu_run = 0;
			} else if (menu_item > 0) {  // switch account
				i = dir_count2 + menu_item - 1;
				size = (dirs[i].size > (reg_init_data->reg_entries[reg_init_data->idx_username].key_size - 1));
				if (!size) {
					struct Registry_Data *reg_new_data;
					struct File_Data file_new_data;

					// clear data part of screen
					psvDebugScreenSetCoordsXY(&x3, &y3);
					printf("\e[0J");
					// initialize data for account to be switched to
					reg_new_data = NULL;
					init_account_reg_data(&reg_new_data);
					init_account_file_data(&file_new_data);
					// copy username
					sceClibStrncpy((char *)(reg_new_data->reg_entries[reg_new_data->idx_username].key_value), dirs[i].name, (reg_new_data->reg_entries[reg_new_data->idx_username].key_size - 1));
					// read account data
					read_account_details(reg_new_data, &file_new_data, reg_init_data, file_init_data);
					// check for sufficient account data (login_id)
					if ((reg_new_data->idx_login_id < 0) || (reg_new_data->reg_entries[reg_new_data->idx_login_id].key_value == NULL) || (sceClibStrnlen((char *)(reg_new_data->reg_entries[reg_new_data->idx_login_id].key_value), 1) == 0))  // check login id
					{
						printf("\e[1mAccount %s data is insufficient (at least login id is needed).\e[22m\e[0K\n", (char *)(reg_new_data->reg_entries[reg_new_data->idx_username].key_value));
						wait_for_cancel_button();
						menu_redraw_screen = 1;
						menu_redraw = 1;
					} else {
						// set account registry data
						set_reg_data(reg_new_data, -1);
						// copy/remove account file data
						set_account_file_data(&file_new_data, reg_new_data->reg_entries[reg_new_data->idx_username].key_value);
						// delete execution history data
						delete_execution_history(&execution_history_data, NULL);
						
						printf("Account %s restored!\e[0K\n", (char *)(reg_new_data->reg_entries[reg_new_data->idx_username].key_value));
						if (switch_saves_folder("ux0:user", (char*)(reg_data->reg_entries[reg_data->idx_username].key_value), (char*)(reg_new_data->reg_entries[reg_new_data->idx_username].key_value)))
							printf("Saves folder swapped!\n");
						else printf("Saves folder didn't swap!\n");
						wait_for_cancel_button();
						menu_run = 0;
						result = 1;
					}
					free_reg_data(reg_new_data);
					free(reg_new_data);
				}
			}
		}
	} while (menu_run);

	// free memory
	free_subdirs(dirs, dir_count);

	return result;
}

int switch_saves_folder(const char* base_path, const char* current_user, const char* new_user) {
    char current_user_path[256];
    char active_user_path[256];
    char new_user_path[256];

    snprintf(current_user_path, sizeof(current_user_path), "%s/00", base_path);
    snprintf(active_user_path, sizeof(active_user_path), "%s/%s", base_path, current_user);
    snprintf(new_user_path, sizeof(new_user_path), "%s/%s", base_path, new_user);

    if (!rename(current_user_path, active_user_path)) {
    	printf("Error renaming directory: %s\n", current_user);
        return 0;
    }

    if (!rename(new_user_path, current_user_path)) {
        printf("Error renaming directory: %s\n", new_user);
        rename(active_user_path, current_user_path);
        return 0;
    }
	return 1;
}

int remove_account(struct Registry_Data *reg_data, struct Registry_Data *reg_init_data, struct File_Data *file_init_data, char *title)
{
	int result;
	int menu_run;
	int menu_items;
	int menu_item;
	int x, y;
	int x3, y3;
	int button_pressed;
	int i;

	result = 0;

	// draw title line
	draw_title_line(title);

	// draw pixel line
	draw_pixel_line(NULL, NULL);
	psvDebugScreenGetCoordsXY(NULL, &y3);  // start of data
	x3 = 0;

	// draw current account data
	display_account_details_short(reg_data, NULL);

	// draw pixel line
	draw_pixel_line(NULL, NULL);

	// draw info
	printf("Remove current account by setting initial account data\e[0K\n");
	printf("in registry and deleting account files.\e[0K\n");
	printf("\e[2K\nContinue?\e[0K\n");

	// draw menu
	psvDebugScreenGetCoordsXY(NULL, &y);
	x = 0;
	menu_run = 1;
	menu_items = 0;
	menu_item = 0;
	printf(" Cancel.\e[0K\n"); menu_items++;
	printf(" Remove account.\e[0K\n");

	do {
		// draw menu marker
		psvDebugScreenSetCoordsXY(&x, &y);
		//
		if (menu_item < 0) {
			menu_item = 0;
		}
		if (menu_item > menu_items) {
			menu_item = menu_items;
		}
		//
		for (i = 0; i <= menu_items; i++) {
			if (menu_item == i) {
				printf(">\n");
			} else {
				printf(" \n");
			}
		}

		// process key strokes
		button_pressed = get_key();
		if (button_pressed == SCE_CTRL_DOWN) {
			menu_item++;
		} else if (button_pressed == SCE_CTRL_UP) {
			menu_item--;
		} else if (button_pressed == button_enter) {
			if (menu_item == 0) {  // cancel
				menu_run = 0;
			} else if (menu_item == 1) {  // remove account
				// clear data part of screen
				psvDebugScreenSetCoordsXY(&x3, &y3);
				printf("\e[0J");
				// set initial account data
				set_reg_data(reg_init_data, -1);
				set_account_file_data(file_init_data, NULL);
				// delete execution history data
				delete_execution_history(&execution_history_data, NULL);
				//
				printf("Account %s removed!\e[0K\n", (char *)(reg_data->reg_entries[reg_data->idx_username].key_value));
				wait_for_cancel_button();
				menu_run = 0;
				result = 1;
			}
		}
	} while (menu_run);

	return result;
}

void main_account(void)
{
	int i;
	char base_path[(MAX_PATH_LENGTH)+1];

	// create accounts base path
	base_path[(MAX_PATH_LENGTH)] = '\0';
	sceClibStrncpy(base_path, app_base_path, (MAX_PATH_LENGTH));
	sceClibStrncat(base_path, accounts_folder, (MAX_PATH_LENGTH));
	create_path(base_path, 0, 0);

	// determine special indexes of registry data
	for (i = 0; i < template_account_reg_data.reg_count; i++) {
		// username
		if ((template_account_reg_data.idx_username < 0) && (template_account_reg_data.reg_entries[i].key_id == reg_id_username)) {
			template_account_reg_data.idx_username = i;
		}
		// login id
		if ((template_account_reg_data.idx_login_id < 0) && (template_account_reg_data.reg_entries[i].key_id == reg_id_login_id)) {
			template_account_reg_data.idx_login_id = i;
		}
		// all found?
		if ((template_account_reg_data.idx_username >= 0) && (template_account_reg_data.idx_login_id >= 0)) {
			break;
		}
	}
}
