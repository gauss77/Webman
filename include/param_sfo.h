enum SFO_Operation_Codes
{
	FIX_SFO,
	SHOW_WARNING,
	GET_TITLE_AND_ID,
	GET_VERSION,
	GET_TITLE_ID_ONLY,
	IS_GAME_DATA
};

static bool is_sfo(unsigned char *mem)
{
	return (mem[1]=='P' && mem[2]=='S' && mem[3]=='F');
}

#define READ_SFO_HEADER(ret) \
	if(is_sfo(mem) == false) return ret; \
	u16 pos, str, fld, dat, siz, typ, indx = 0; \
	fld=str=(mem[0x8]+(mem[0x9]<<8)); \
	dat=pos=(mem[0xc]+(mem[0xd]<<8)); \

#define FOR_EACH_SFO_FIELD() \
	while(pos < sfo_size) \
	{ \
		if((str>=dat) || (mem[str]==0)) break; \
		siz=mem[0x1c+indx]+(mem[0x1d+indx]<<8); \
		typ=mem[0x17+indx];

#define READ_NEXT_SFO_FIELD() \
		while((str < dat) && mem[str]) str++;str++; \
		pos+=siz, indx+=0x10; if(indx>=fld) break; \
	}

static void parse_param_sfo(unsigned char *mem, char *title_id, char *title, u16 sfo_size)
{
	READ_SFO_HEADER()

	_memset(title_id, 10);
	_memset(title, 128);

	FOR_EACH_SFO_FIELD()
	{
		if(!memcmp((char *) &mem[str], "TITLE_ID", 8))
		{
			strncpy(title_id, (char *)mem + pos, TITLE_ID_LEN);
			#ifndef ENGLISH_ONLY
			if(*TITLE_XX == NULL)
			#endif
				if(*title) break;
		}
		else
		if(!memcmp((char *) &mem[str], "TITLE", 6))
		{
			strncpy(title, (char *)mem + pos, 128);
			#ifndef ENGLISH_ONLY
			if(*TITLE_XX == NULL)
			#endif
				if(*title_id) break;
		}
		#ifndef ENGLISH_ONLY
		else
		if(!memcmp((char *) &mem[str], TITLE_XX, TITLE_ID_LEN))
		{
			strncpy(title, (char *)mem + pos, 128);
			if(*title_id) break;
		}
		#endif
		READ_NEXT_SFO_FIELD()
	}

	if(webman_config->tid && ISDIGIT(title_id[8]) && (*title_id == 'B' || *title_id == 'N' || *title_id == 'S'))
	{
		strcat(title, " ["); strcat(title, title_id); strcat(title, "]");
	}
}

#ifdef UNLOCK_SAVEDATA
static u8 patch_param_sfo(const char *param_sfo, unsigned char *mem, u16 sfo_size, u32 attribute)
{
	if(!strstr(param_sfo, "/PARAM.SFO")) return false;

	if(not_exists(param_sfo)) return false;

	u8 save = 0;

	if(*mem == NULL) read_file(param_sfo, (void*)mem, sfo_size, 0);

	READ_SFO_HEADER(false)

	FOR_EACH_SFO_FIELD()
	{
		if(!memcmp((char *) &mem[str], "ACCOUNT_ID", 10))
		{
			_memset((char *) &mem[pos], 16); save++;
		}
		else if(!memcmp((char *) &mem[str], "ACCOUNTID", 9))
		{
			_memset((char *) &mem[pos], 16); save++;
		}
		else if(!memcmp((char *) &mem[str], "ATTRIBUTE", 9))
		{
			memcpy((char *) &mem[pos], &attribute, sizeof(u32)); save++; // set attribute (0 = Remove Copy Protection)
		}
		else if(!memcmp((char *) &mem[str], "PARAMS", 6))
		{
			int userid = xusers()->GetCurrentUserNumber();
			mem[pos + 24] = (u8)(userid);           // User 1
			_memset((char *) &mem[pos + 28], 16); // PSID
			_memset((char *) &mem[pos + 48], 16); // Account ID
			if(++save >= 3) break;
		}
		READ_NEXT_SFO_FIELD()
	}

	return save;
}

static u8 unlock_param_sfo(const char *param_sfo, unsigned char *mem, u16 sfo_size)
{
	if( islike(param_sfo, HDD0_HOME_DIR) == false) return false;
	return patch_param_sfo(param_sfo, mem, sfo_size, 0);
}
#endif

#ifdef FIX_GAME
static bool fix_param_sfo(unsigned char *mem, char *title_id, u8 opcode, u16 sfo_size)
{
	READ_SFO_HEADER(false)

	_memset(title_id, 10);

	#ifdef FIX_GAME
	u8 fcount = 0;
	#endif

	bool ret = false;

	FOR_EACH_SFO_FIELD()
	{
		if(!memcmp((char *) &mem[str], "TITLE_ID", 8))
		{
			strncpy(title_id, (char *) &mem[pos], TITLE_ID_LEN);
			#ifdef FIX_GAME
			if(opcode == GET_TITLE_ID_ONLY) break;
			fcount++; if(fcount >= 2) break;
			#else
			break;
			#endif
		}
		else
		if(!memcmp((char *) &mem[str], "PS3_SYSTEM_VER", 14))
		{
			char version[8];
			strncpy(version, (char *) &mem[pos], 7);
			int fw_ver = 10000 * ((version[1] & 0x0F)) + 1000 * ((version[3] & 0x0F)) + 100 * ((version[4] & 0x0F));
			if((c_firmware >= 4.20f && c_firmware < LATEST_CFW) && (fw_ver > (int)(c_firmware * 10000.0f)))
			{
				if(opcode == SHOW_WARNING) {char text[64]; sprintf(text, "WARNING: Game requires firmware version %i.%i", (fw_ver / 10000), (fw_ver - 10000*(fw_ver / 10000)) / 100); show_msg(text); break;}

				mem[pos + 1] = '4', mem[pos + 3] = '2', mem[pos + 4] = '0'; ret = true;
			}
			fcount++; if(fcount >= 2) break;
		}

		READ_NEXT_SFO_FIELD()
	}

	return ret;
}
#endif

#ifdef VIEW_PARAM_SFO
static u64 c2b(unsigned char *mem, u8 len)
{
	u64 value = 0;
	for(u8 b = 0; b < len; b+=8, mem++)
		value += *mem<<b;
	return value;
}
#endif

static void get_param_sfo(unsigned char *mem, const char *field, char *value, u8 value_len, u16 sfo_size)
{
	READ_SFO_HEADER()

	#ifdef VIEW_PARAM_SFO
	u16 len = 0;
	if(!field)
		len = sprintf(value, "<table cellspacing=8 width=800>");
	else
		len = strlen(field);
	#else
	u8 len = strlen(field);
	#endif

	FOR_EACH_SFO_FIELD()
	{
		#ifdef VIEW_PARAM_SFO
		if(!field)
		{
			len += 30; if(len + siz >= 5500) break;
			concat(value, "<tr><td align=top>");
			len += concat(value, (char*)&mem[str]);
			concat(value, "<td>");
			if(typ==4)
			{
				char tmp[20];
				if(siz==8)
					{len += sprintf(tmp, "0x%016llX", c2b(&mem[pos],64)); concat(value, tmp);}
				else
					{len += sprintf(tmp, "0x%08llX", c2b(&mem[pos],32)); concat(value, tmp);}
			}
			else
				len += concat(value, (char*)&mem[pos]);
			concat(value, "</tr>");
		}
		else
		#endif
		if(!memcmp((char *) &mem[str], field, len))
		{
			strncpy(value, (char *) &mem[pos], value_len);
			break;
		}

		READ_NEXT_SFO_FIELD()
	}
	#ifdef VIEW_PARAM_SFO
	if(!field)
		concat(value, "</table>");
	#endif
}

static bool getTitleID(char *filename, char *title_id, u8 opcode)
{
	bool ret = false;

	_memset(title_id, 10);

	char param_sfo[_4KB_];
	unsigned char *mem = (u8*)param_sfo;

	check_ps3_game(filename);

	u16 sfo_size = read_sfo(filename, param_sfo);

	if(sfo_size)
	{
		// get titleid
		if(opcode == GET_VERSION)
		{
			char *app_ver = title_id;
			get_param_sfo(mem, "APP_VER", app_ver, 6, sfo_size);   // get game version (app_ver)
		}
		else if(opcode == IS_GAME_DATA)
		{
			char *category = title_id;
			get_param_sfo(mem, "CATEGORY", category, 4, sfo_size); // get category & return if it is GD
			return islike(category, "GD");
		}
		else if(opcode == GET_TITLE_AND_ID)
			parse_param_sfo(mem, title_id, filename, sfo_size);    // get titleid & return title in the file name (used to backup games in _mount.h)
		#ifdef FIX_GAME
		else
		{
			ret = fix_param_sfo(mem, title_id, opcode, sfo_size);  // get titleid & show warning if game needs to fix PS3_SYSTEM_VER

			if(ret && opcode == FIX_SFO) save_file(filename, param_sfo, sfo_size);
		}
		#endif
	}

	return ret;
}

static void set_param_sfo(char *path)
{
	strcpy(path, "/dev_bdvd/PS3_GAME/PARAM.SFO");
	if(not_exists(path)) strcpy(path, "/app_home/PS3_GAME/PARAM.SFO");
	if(not_exists(path)) strcpy(path, "/app_home/PARAM.SFO");
}

static void get_local_app_ver(char *app_ver, char *title_id, char *param_sfo)
{
	if(*title_id)
	{
		sprintf(param_sfo, "%s/%s/PARAM.SFO", HDD0_GAME_DIR, title_id);
		if(not_exists(param_sfo)) return;

		char app_ver2[8]; *app_ver2 = NULL;
		getTitleID(param_sfo, app_ver2, GET_VERSION); if(*app_ver2) strcpy(app_ver, app_ver2);
	}
}

static void get_game_version(char *sfo, char *title, char *title_id, char *app_ver)
{
	*title = *title_id = *app_ver = NULL;

	if(get_game_info())
	{
		strcpy(title, _game_Title);
		strcpy(title_id, _game_TitleID);
	}
	else
	{
		strcpy(title, sfo);
		getTitleID(title, title_id, GET_TITLE_AND_ID);
	}

	if(file_exists(sfo))
		getTitleID(sfo, app_ver, GET_VERSION);

	get_local_app_ver(app_ver, title_id, sfo);
}
