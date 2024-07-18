// Copyright (c) rAthena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder

/**
 * Custom Script Commands
 * Place the body of custom script commands in this file.
 * Format:
 *	BUILDIN_FUNC(command_name)
 *	{
 *		<code>;
 *	}
 **/

//BUILDIN_FUNC(example)
//{
//	ShowInfo("buildin_example: It works!\n");
//	script_pushint(st,1);
//	return 0;
//}

//--------------------------------------
//DynamicNPC NoName
BUILDIN_FUNC(duplicatenpc)
{
	int class_ = -1;
	int sourceid = -1;
	int mapid = -1;

	const char *sourcename		= script_getstr(st, 2);
	const char *new_shown_name	= script_getstr(st, 3);
	const char *new_hidden_name = script_getstr(st, 4);
	const char *mapname			= script_getstr(st, 5);

	int map_x					= script_getnum(st, 6);
	int map_y					= script_getnum(st, 7);
	int dir						= script_getnum(st, 8);


	char	new_npc_name[NPC_NAME_LENGTH] = "";
	struct	npc_data *nd_source			=	nullptr;
	struct	npc_data *nd_target			=	nullptr;

	if(strlen(new_shown_name) + strlen(new_hidden_name) > NPC_NAME_LENGTH)
	{
		ShowError("buildin_duplicatenpc: NPC name is too long. (MAX length: %d). (%s)\n", NPC_NAME_LENGTH,sourcename);
		return SCRIPT_CMD_FAILURE;
	}

	strcat(new_npc_name, new_shown_name);
	strncat(new_npc_name, "#", 1);
	strncat(new_npc_name, new_hidden_name, strlen(new_hidden_name));
	
	
	TBL_PC *sd = NULL;
	script_rid2sd(sd);

	nd_source = npc_name2id(new_npc_name);
	if(nd_source && sd->bl.m != nd_source->bl.m)
	{
		if(!nd_source->src_id)
			npc_unload_duplicates(nd_source);
		else
			npc_unload(nd_source,true);
	
		nd_source = nullptr;
	}

	if(!npc_name2id(new_npc_name)) {
		nd_source = npc_name2id(sourcename);
	
		if(nd_source==nullptr)
		{
			ShowError("buildin_duplicatenpc: Can't find original NPC (%s)\n", sourcename);
			return SCRIPT_CMD_FAILURE;
		}

		class_ = nd_source->class_;
	
		sourceid = nd_source->bl.id;
		mapid = map_mapname2mapid(mapname);

		if(mapid < 0)
		{
			ShowError("buildin_duplicatenpc: Can't find map (%s)\n", mapname);
			return SCRIPT_CMD_FAILURE;
		}

		CREATE(nd_target, struct npc_data, 1);
	

		safestrncpy(nd_target->name, new_npc_name , sizeof(nd_target->name));
		safestrncpy(nd_target->exname, new_npc_name, sizeof(nd_target->exname));

		nd_target->bl.prev = nd_target->bl.next = NULL;
		nd_target->bl.m = mapid;
		nd_target->bl.x = map_x;
		nd_target->bl.y = map_y;
		nd_target->bl.id = npc_get_new_npc_id();
		nd_target->class_ = class_;
		nd_target->speed = 200;
		nd_target->src_id = sourceid;
		nd_target->bl.type = BL_NPC;
		nd_target->subtype = nd_source->subtype;

		switch(nd_source->subtype)
		{
			case NPCTYPE_SCRIPT:
				nd_target->u.scr.xs				= nd_source->u.scr.xs;
				nd_target->u.scr.ys				= nd_source->u.scr.ys;
				nd_target->u.scr.script			= nd_source->u.scr.script;
				nd_target->u.scr.label_list		= nd_source->u.scr.label_list;
				nd_target->u.scr.label_list_num = nd_source->u.scr.label_list_num;
				break;
			case NPCTYPE_SHOP:
			case NPCTYPE_CASHSHOP:
			case NPCTYPE_ITEMSHOP:
			case NPCTYPE_POINTSHOP:
			case NPCTYPE_MARKETSHOP:
				nd_target->u.shop.shop_item		= nd_source->u.shop.shop_item;
				nd_target->u.shop.count			= nd_source->u.shop.count;
				break;
			case NPCTYPE_WARP:
				if( !battle_config.warp_point_debug )
					nd_target->class_ = JT_WARPNPC;
				else
					nd_target->class_ = JT_GUILD_FLAG;
				nd_target->u.warp.xs		= nd_source->u.warp.xs;
				nd_target->u.warp.ys		= nd_source->u.warp.ys;
				nd_target->u.warp.mapindex	= nd_source->u.warp.mapindex;
				nd_target->u.warp.x			= nd_source->u.warp.x;
				nd_target->u.warp.y			= nd_source->u.warp.y;
				nd_target->trigger_on_hidden= nd_source->trigger_on_hidden;
				break;
		}

		map_addnpc(mapid, nd_target);
		status_change_init(&nd_target->bl);
		unit_dataset(&nd_target->bl);
		nd_target->ud.dir = dir;
		npc_setcells(nd_target);
		map_addblock(&nd_target->bl);

		if(class_ >= 0) {
			status_set_viewdata(&nd_target->bl, nd_target->class_);
			clif_spawn(&nd_target->bl);
		}

		npc_noname_duplicate(sd,nd_target);

		nd_target->dynamicnpc.owner_char_id = sd->status.char_id;
		nd_target->dynamicnpc.last_interaction = gettick();
		nd_target->dynamicnpc.removal_tid = add_timer( nd_target->dynamicnpc.last_interaction + battle_config.feature_dynamicnpc_timeout, npc_dynamicnpc_removal_timer, nd_target->bl.id, (intptr_t)nullptr );
	} else	{
		clif_dynamicnpc_result( *sd, DYNAMICNPC_RESULT_DUPLICATE );
	}

	char* buf = (char *)aMalloc(strlen(new_npc_name)+1);
	sprintf(buf, new_npc_name);
	script_pushstr(st, buf);
	return SCRIPT_CMD_SUCCESS;
}

BUILDIN_FUNC(duplicateremove)
{
	struct npc_data *nd = nullptr;

	if(script_hasdata(st, 2))
	{
		nd = npc_name2id(script_getstr(st, 2));
		if(nd == nullptr)
		{
			script_pushint(st, -1);
			return SCRIPT_CMD_FAILURE;
		}
	}
	else
		nd = (struct npc_data *)map_id2bl(st->oid);

	if(!nd->src_id)
		npc_unload_duplicates(nd);
	else
		npc_unload(nd,true);

	script_pushint(st, 1);
	return SCRIPT_CMD_SUCCESS;
}
