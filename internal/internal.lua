--BACKWARD COMPAT SECTION

-- deprecated functions that are still used
dsb_use_wallset = dsb_level_wallset
dsb_replace_portrait = dsb_set_portraitname
dsb_alt_wallset = dsb_set_alt_wallset
dsb_lastmethod = dsb_get_lastmethod

-- fix things if loading old files
function __lua_backward_compat_fixes(filever, curver)
	-- An old save file uses the old light system
	if (filever <= 46) then
	    __DSB_LIGHT_HANDLES = {
	        magic = __get_light_raw(0),
			torches = __get_light_raw(1),
			illumulets = __get_light_raw(2)
		}
		
		__set_light_raw(1, 0)
		__set_light_raw(2, 0)
		
		__set_light_raw(0, __DSB_LIGHT_HANDLES.magic +
		    __DSB_LIGHT_HANDLES.torches +
			__DSB_LIGHT_HANDLES.illumulets)
	end
	
	-- Just in case someone redefined magic the old way
	if (filever <= 53) then
		for msys in pairs(magic_info) do
			if (_G[msys]) then
			    magic_info[msys] = _G[msys]
			end
		end
	end
	
	-- Repair magic light
	if (filever > 46 and filever <= 63) then
		dsb_set_light(0, 0)
		for i in dsb_insts() do
			local arch = dsb_find_arch(i)
			if (arch == obj.light_controller) then
				dsb_msg(0, i, M_DESTROY, 0) 
			end
		end
		local magic_light = dsb_get_light("magic")
		if (magic_light > 0) then
			local lc = dsb_spawn("light_controller", LIMBO, 0, 0, 0)
			exvar[lc] = { light = magic_light }
			local rsb = 5*dsb_rand(30, 600)
			dsb_msg(rsb, lc, M_NEXTTICK, 6)
			__log("Applying light duration fix [brightness = " .. magic_light .. ", time = " .. rsb .. "] to v0.46-0.63 savegame")
		end
		
	end
	
end


-- UTILITY FUNCTIONS

function __register_objects(zcount)
	local ct
	
	__trt_iassign()
	
	for ct=1,(zcount-1) do
		__regobject(__ZIDT[ct])
	end
	
	__trt_purge()	
	for ct=1,(zcount-1) do
		__trt_obj_writeback(__ZIDT[ct])
	end		
end

function __force_sane(fs, default, min, max)
	if (fs) then
	    if (type(fs) ~= "number") then
	    	return default
		end
		
		if (min and fs < min) then
			return min
		end
		
		if (max and fs > max) then
			return max
		end
		
		return fs
	end
	return default
end

function __gui_importation()
	local curzone = 3
	local res_zones = {
		viewport = "VIEWPORT",
		current_item = -1,
		portraits = -2,
		console = -3,
		guy_icons = -4,
		methods = 0,
		magic = 1,
		movement = 2
	}
	
	if (not gui_info or type(gui_info) ~= "table") then
		return
	end
	
	local i
	for i in pairs(gui_info) do
		local x, y, w, h
		local zid
		if (res_zones[i]) then
			zid = res_zones[i]
		else
			zid = curzone
			curzone = curzone + 1
		end
		
		if (type(gui_info[i]) == "table") then	
			x = gui_info[i].x
			if (not x) then x = 0 end

			y = gui_info[i].y
			if (not y) then y = 0 end
			
			w = gui_info[i].w
			if (not w or w < 1) then w = 1 end

			h = gui_info[i].h
			if (not h or h < 1) then h = 1 end
			
			if (type(zid) == "string") then
				if (zid == "VIEWPORT") then
					local yoff = 0
					
					if (gui_info[i].y_off) then
						yoff = gui_info[i].y_off
						if (type(yoff) ~= "number") then
						    yoff = 0
						end
					end
					 
					dsb_viewport(x, y, yoff)
				end
			else
			    local xflags = 0
			    
			    if (zid == res_zones.current_item) then
			        if (w > 1 and h > 1) then
						__activate_gui_zone(i, curzone, x, y, w, h, 2)
						__set_internal_gui("itemname_drawzone", curzone)
						curzone = curzone + 1
					end
			    end
			    
				__activate_gui_zone(i, zid, x, y, w, h, 0)
				
				if (zid == res_zones.console) then
				    local conlines = __force_sane(gui_info[i].lines, 4, 1, 16)
					__set_internal_gui("console", conlines)
				end
					
			end
			
		end 	
	end
end

function __import_shading_info()
	local sh_strings = { "WALLITEM", "FLOORFLAT", "FLOORUPRIGHT",
		"MONSTER", "THING", "DOOR", "CLOUD" }
	local d_strings = { "front", "pers" }
	
	for i=1,7 do
		local sstr = sh_strings[i]
		local vtable
		
		for d=1,2 do
			local dstr = d_strings[d]
			
			vtable = {0, 64, 64}
			
			if (shading_info and type(shading_info) == "table") then
				if (shading_info[sstr]) then
					local ssinfo = shading_info[sstr]
					if (ssinfo and type(ssinfo) == "table") then
						local ssdinfo = shading_info[sstr][dstr]
						if (ssdinfo and type(ssdinfo) == "table") then
							vtable = { }
							vtable[1] = ssdinfo[1]
							vtable[2] = ssdinfo[2]
							vtable[3] = ssdinfo[3]
						end
					end
				end
			end
			
			__set_internal_shadeinfo(i, d, vtable[1], vtable[2], vtable[3])
						
		end		
	end
end

function __import_arch_shading_info(arch_id)
	if (arch_id and obj[arch_id] and obj[arch_id].shading_info) then
		local arch_sinfo = obj[arch_id].shading_info
		if (type(arch_sinfo) == "table") then
			local d_strings = { "front", "pers" }
			for d=1,2 do
				local dstr = d_strings[d]
				local vtable = { 0, 0, 0 }
				local ssdinfo = arch_sinfo[dstr]
				if (ssdinfo and type(ssdinfo) == "table") then
					vtable = { }
					vtable[1] = ssdinfo[1]
					vtable[2] = ssdinfo[2]
					vtable[3] = ssdinfo[3]
				end
				__set_internal_shadeinfo(1, d, vtable[1], vtable[2], vtable[3], arch_id)
			end
		end
	end
end

function dsb_clean_up_target_exvars(badid)
	
	if (not badid) then return end
	
	for nc=1,32768 do  
		if (exvar[nc]) then
			if (exvar[nc].target == badid) then
				exvar[nc].target = nil
				exvar[nc].msg = nil
			else
				if (type(exvar[nc].target) == "table") then
					for si=1,99 do
						if (exvar[nc].target[si] == nil) then break end
						if (exvar[nc].target[si] == badid) then
							exvar[nc].target[si] = 0	
						end
					end
					
					local swaps = 1
					while (swaps > 0) do
						swaps = 0
						for si=2,99 do
							if (exvar[nc].target[si] == nil) then break end
							if (exvar[nc].target[si] ~= 0 and
								exvar[nc].target[si-1] == 0)
							then
								exvar[nc].target[si-1] = exvar[nc].target[si]
								exvar[nc].target[si] = 0 
								swaps = swaps + 1
								
								if (type(exvar[nc].delay) == "table") then
									exvar[nc].delay[si-1] = exvar[nc].delay[si]
									exvar[nc].delay[si] = 0 
								end	
								
								if (type(exvar[nc].data) == "table") then
									exvar[nc].data[si-1] = exvar[nc].data[si]
									exvar[nc].data[si] = 0 
								end	
								
								if (type(exvar[nc].msg) == "table") then
									exvar[nc].msg[si-1] = exvar[nc].msg[si]
									exvar[nc].msg[si] = 0 
								end	
							end
						end
					end
					
					local lnonzero = 0
					for si=1,255 do
						if (exvar[nc].target[si] == nil) then break end
						if (exvar[nc].target[si] ~= 0) then
							lnonzero = si	
						end
					end
					
					for si=lnonzero+1,255 do
					
						exvar[nc].target[si] = nil
						
						if (type(exvar[nc].delay) == "table") then
							exvar[nc].delay[si] = nil
						end	
						if (type(exvar[nc].data) == "table") then
							exvar[nc].data[si] = nil
						end								
						if (type(exvar[nc].msg) == "table") then
							exvar[nc].msg[si] = nil
						end	
					end										
					
				end
			end
		end
	end	
end	

function __wallset_name_by_ref(refnum)
	for idx in pairs(wallset) do
		if (wallset[idx].ref_int == refnum) then
		    return idx
		end
	end
	return nil
end

function __object_table(typecode)
	local classes = { }
	local c_membs = { }
	local ot = { }
	local cn = 1
		
	for i in pairs(obj) do
		v = obj[i]
		if (typecode == v.type) then
			local cl = v.class
			if (not cl) then cl = "none" end
			if (not ot[cl]) then 
				ot[cl] = { }
				classes[cn] = cl
				c_membs[cl] = 0
				cn = cn + 1
			end
			
			c_membs[cl] = c_membs[cl] + 1
			ot[cl][c_membs[cl]] = i
		end
	end	
	
	return ot, classes	
end

function __table_dump(tx)
	local j
	local u = 1
	
	if (not tx) then
		_TXVT = { _ELE = 0 }
		return nil
	end
	
	_TXVT = { }
	for j in pairs(tx) do
		_TXVT[u] = j
		u = u + 1
	end
	
	_TXVT._ELE = (u-1)
	
end

function __exvar_dump(xv, v)
	local j
	local u = 1
	
	if (not xv[v]) then
		_EXVT = { _ELE = 0 }
		return nil
	end
	
	_EXVT = { }
	for j in pairs(xv[v]) do
		_EXVT[u] = j
		u = u + 1
	end
	
	_EXVT._ELE = (u-1)
	
end

function __obj_mt()
	obj = { }
	__ZIDT = { }
	obj_mt = { __newindex = __obj_newidxfunc }
	setmetatable(obj, obj_mt)
end

function __no_index(tbl, key)
	crap = { tblkey = key }
	return crap
end

function __log_index(tbl, key)
	return key
end

function __editor_metatables()
	gfx = { }
	snd = { }
	wallset = { }
	
	mt = { }
    mt.__index = __no_index	
	setmetatable(gfx, mt)
	setmetatable(snd, mt)
	
	wt =  { }
	wt.__index = __log_index
	setmetatable(wallset, wt)
	
	__C = dsb_add_champion
	__I = dsb_spawn
	_ESBCNAME = { }
	
	_ESBWALLSET_NAMES = { }
	_ESBLEVELUSE_WALLSETS = { }
	reverse_champion_table = { }
	
	EDITOR_FLAGS = 255
end

function __editor_fix_wallsets() 
	_ESB_WALLSETS_USED["default"] = true
	for l=0,ESB_LAST_LEVEL do
		if (not _ESBWALLSET_NAMES[l]) then
			_ESBWALLSET_NAMES[l] = "default"
		end
	end
end

function __ed_ws_drawpack(lev, pack, new_wallset)
	if (new_wallset == _ESBWALLSET_NAMES[lev]) then
	    _ESBLEVELUSE_WALLSETS[pack] = nil
	else
	    _ESBLEVELUSE_WALLSETS[pack] = new_wallset
	end
end

function __clearmanifest()
	lua_manifest = nil
end

function __void_exvar(id)
	if (exvar and exvar[id]) then
	    exvar[id] = nil
	end
end

function __sconvert(i, cte)
	if ((i == "msg" or i == "send_msg") and msg_types[cte]) then
		return msg_types[cte]
	elseif (i == "champion" and reverse_champion_table[cte]) then
		return reverse_champion_table[cte]
	elseif (type(cte) == "string") then
		return "\"" .. tostring(cte) .. "\""
	elseif (type(cte) == "function") then
		return "0"
	else
		return tostring(cte)
	end
end

function __ppcheckserialize(extbl)
	if (obj and extbl == obj) then
		return "{@OBJ}"
	elseif (gfx and extbl == gfx) then
		return "{@GFX}"
	elseif (wallset and extbl == wallset) then
		return "{@WALLSET}"
	elseif (snd and extbl == snd) then
		return "{@SND}"
	elseif (exvar and extbl == exvar) then
		return "{@EXVAR}"
	elseif (ch_exvar and extbl == ch_exvar) then
		return "{@CH_EXVAR}"
	elseif (obj and extbl and extbl.type and extbl.class) then
		if (extbl.name) then
			return "{@OBJ[" .. extbl.name .. "]}"
		else
			return "{@OBJ[?]:(" .. extbl.type .. "/" .. extbl.class .. ")}"
		end
	elseif (extbl == _G) then
		return "{@_G}"
	elseif (type(extbl) == "table") then
		local cvstr = "{ "
		for i in pairs(extbl) do
			cvstr = cvstr .. i .. " "
		end
		cvstr = cvstr .. "}"
		return cvstr		
	else
		return __ppserialize(extbl, "")
	end
end     	

function __ppserialize(extbl, i)
	local csvar = "<UNKNOWN>"
	if (type(extbl) == "table") then
		csvar = "{ "
		local ctbl = extbl
		if (ctbl[1] ~= nil) then
			for ti=1,99 do
				local cte = ctbl[ti]
				if (type(cte) == "table") then
					cvr = "{ ... }"
				else cvr = __sconvert(i, cte) end					
				csvar = csvar .. cvr
				if (ctbl[ti+1] == nil) then
					csvar = csvar .. " }"
					break
				else csvar = csvar .. ", " end
			end
		else
			csvar = csvar .. "... }"
		end	
	else
		csvar = __sconvert(i, extbl)
	end
	
	return csvar
end

function __ppexvar(inst, i, prep)
	local csvar = __ppserialize(exvar[inst][i], i)
	if (prep) then __l_tag(i .. " = " .. csvar)
	else __l_tag(csvar) end
end

function __ppchexvar(inst, i, prep)
	local csvar = __ppserialize(ch_exvar[inst][i], i)
	if (prep) then __l_tag(i .. " = " .. csvar)
	else __l_tag(csvar) end
end

function __serialize(sep, fieldname, value, recurs)
	local nrecval = 0
	if (recurs) then nrecval = recurs end
	nrecval = nrecval + 1
	if (nrecval > 7) then
		return "\"...\""
	end
	if (type(value) == "table") then
		local astr = "{ "
		local handled = { }
		local numf = 1
		local first = true
		local cr = false
		while (value[numf] ~= nil) do
			if (not first) then
				astr = astr .. ", "	
			end
			first = false 
			astr = astr .. __serialize(sep, fieldname, value[numf], nrecval)
			handled[numf] = true
			numf = numf + 1
		end
		for otx in pairs(value) do
			if (not handled[otx]) then
				local index 
				if (type(otx) == "number") then
					index = "[" .. tostring(otx) .. "]"
				else
					index = tostring(otx)
				end
				if (not first) then
					astr = astr .. "," .. sep
					cr = true
				end
				first = false
				
				astr = astr .. index .. " = " .. __serialize(sep, fieldname, value[otx], nrecval)
			end
		end
		astr = astr .. " }"
		return astr
	else
		return __sconvert(fieldname, value)
	end
end

function __ed_fillexvar(inst)
	if (exvar[inst]) then
		local ex_tagged = { }
		for c=1,300 do
			local impx = important_exvars[c]
			if (impx == nil) then break end
			if (exvar[inst][impx] or exvar[inst][impx] == false) then
				ex_tagged[impx] = true
				__ppexvar(inst, impx, true)
			end	
		end
		for i in pairs(exvar[inst]) do
			if (not ex_tagged[i]) then
				__ppexvar(inst, i, true)
			end
		end
	end
end

function __ed_fillchexvar(ch)
	if (ch_exvar[ch]) then
		for i in pairs(ch_exvar[ch]) do
			__ppchexvar(ch, i, true)
		end
	end
end

function __init_ch_exvar_editing(ch)
	if (not ch_exvar[ch]) then
		ch_exvar[ch] = { }
	end
end

function __finish_ch_exvar_editing(ch)
	if (ch_exvar[ch] and ch_exvar[ch] == { }) then
		ch_exvar[ch] = nil
	end
end


function __fetchmsgdelaydata(arch, spc, idx, msg, delay, datatable)
	local m, d, dt
	if (type(msg) == "table") then
		m = msg[idx]
	else
		m = msg
	end
	if (not m and not spc) then
		if (arch.default_msg) then
		    m = arch.default_msg
		else
		    m = M_TOGGLE
		end
	end
	
	if (type(delay) == "table") then
		d = delay[idx]
	elseif (delay == nil) then
		d = 0
	else
		d = delay
	end
	
	if (type(datatable) == "table") then
		dt = datatable[idx]
	else
		dt = datatable
	end
	
	return m, d, dt
end

function __ed_fill_userdefined_msgs()
	local udeft = 0
	for mt in pairs(msg_types) do
		if (mt < 100000) then
			__l_tag(msg_types[mt])
			udeft = udeft + 1
		end
	end
	return udeft
end

function __ed_user_def_string_msg(msg_string)
	local umsg = -1
	if (_G[msg_string]) then
		umsg = _G[msg_string]
	end
	return umsg
end

function __ed_target_exvars_to_ttargtable(inst)
	local spc = false
	__TTARGTABLE = { }
	
	local arch = dsb_find_arch(inst)
	if (arch.esb_special_trigger) then
		spc = true
	end
	
	if (not exvar[inst] or not exvar[inst].target) then return end
	if (type(exvar[inst].target) == "table") then
		for z=1,99 do
			if (exvar[inst].target[z] == nil) then break end
			local msg, delay, data = __fetchmsgdelaydata(arch, spc, z, exvar[inst].msg, exvar[inst].delay, exvar[inst].data)
			__TTARGTABLE[z] = { exvar[inst].target[z], msg, delay, data }
		end
	else
		local msg, delay, data = __fetchmsgdelaydata(arch, spc, 1, exvar[inst].msg, exvar[inst].delay, exvar[inst].data)
		__TTARGTABLE[1] = { exvar[inst].target, msg, delay, data }
	end 
end

function __ed_fill_from_ttargtable(id)
	local arch = dsb_find_arch(id)
	for z=1,199 do
		if (__TTARGTABLE[z] == nil) then break end
		
		local cstr
		local msg = __TTARGTABLE[z][2]
		if (msg == nil) then
			cstr = "<Special> "
		elseif (msg_types[msg]) then
			cstr = msg_types[msg] .. " to "
		else
		    cstr = "(" .. msg .. ") to "
		end
		
		local inst_id = __TTARGTABLE[z][1]
		if (dsb_valid_inst(inst_id)) then
			local inst_arch = dsb_find_arch(inst_id)
			local arch_name = inst_arch.ARCH_NAME
			cstr = cstr .. inst_id .. "/" .. arch_name
		else
		    cstr = cstr .. inst_id .. "/???"
		end
		
		if (__TTARGTABLE[z][4]) then
			cstr = cstr .. ":[" .. __TTARGTABLE[z][4] .. "]"
		end
		
		if (__TTARGTABLE[z][3] and __TTARGTABLE[z][3] > 0) then
			cstr = cstr .. " (Delay " .. __TTARGTABLE[z][3] .. ")"
		end
		__l_tag(cstr)
	end
end

function __ed_get_ttargeted_inst(selfield)
	if (__TTARGTABLE[selfield]) then
	    local targ_inst = __TTARGTABLE[selfield][1]
	    if (targ_inst and dsb_valid_inst(targ_inst)) then
			return targ_inst
		else
		    return 0
		end
	else
		return 0
	end
end

function __ed_new_ttargeted_inst(inst, msg, delay, data)
	if (msg <= 0) then msg = nil end
	for z=1,199 do
		if (__TTARGTABLE[z] == nil) then
			__TTARGTABLE[z] = { inst, msg, delay, data }
		  	return
		end
	end
end

function __ed_retarget_ttargeted_inst(selfield, msg, delay, data)
	local inst = __ed_get_ttargeted_inst(selfield)
	if (__TTARGTABLE[selfield] and inst > 0) then
		__TTARGTABLE[selfield] = { inst, msg, delay, data }
		return
	end
end

function __ed_remove_ttargeted_inst(selfield)
	if (__TTARGTABLE[selfield]) then
		__TTARGTABLE[selfield] = nil
	else
		return
	end
	
	local swaps = 1
	while (swaps > 0) do
		swaps = 0
		for z=2,199 do
			if (__TTARGTABLE[z] and not __TTARGTABLE[z-1]) then
				__TTARGTABLE[z-1] = __TTARGTABLE[z]
				__TTARGTABLE[z] = nil
				swaps = swaps + 1
			end
		end
	end
		
end

function __ed_add_wallsets(zp)
	if (_ESB_WALLSETS_USED["default"])
		then __l_tag("default") end
		
	for i in pairs(_ESB_WALLSETS_USED) do
		if (i ~= "default") then
			__l_tag(i)
		end
	end
end

function __ed_wallset_usage_check(ws)
	for i in pairs(_ESBWALLSET_NAMES) do
		if (_ESBWALLSET_NAMES[i] == ws) then
			return 1
		end	
	end
	for i in pairs(_ESBLEVELUSE_WALLSETS) do
		if (_ESBLEVELUSE_WALLSETS[i] == ws) then
			return 1
		end	
	end
	
	return nil
end

function __ed_getalltargs(lev, x, y)
	local fetches = 0
	local nz = 0
	local t_targeted = { }
	local ltargets = { }
	local lmsgs = { }
	
	 if (lev < 0) then
	 	return 0, nil, nil
	end
	
	for d=0,4 do
		local f = dsb_fetch(lev, x, y, d)
		if (f) then
			fetches = fetches + 1
			for i in pairs(f) do
				local iinst = f[i]
				if (exvar[iinst]) then
					local imsg = nil
					if (exvar[iinst].msg) then
						imsg = exvar[iinst].msg
					end
					if (exvar[iinst].target) then
						local it = exvar[iinst].target
						local iinst_arch = dsb_find_arch(iinst)
						if (type(it) == "table") then
							for z=1,199 do
								if (it[z]) then
									if (type(imsg) == "table") then
										local mc = editor_msg_draw_color(imsg[z])
										t_targeted[it[z]] = esb_makecol(mc)
									elseif (imsg) then
										local mc = editor_msg_draw_color(imsg)
										t_targeted[it[z]] = esb_makecol(mc)
									elseif (iinst_arch.esb_targ_draw_color) then
										t_targeted[it[z]] = esb_makecol(iinst_arch.esb_targ_draw_color)
									else
										t_targeted[it[z]] = esb_makecol(255, 255, 255)
									end
								else
								    break
								end
							end
						else
							if (type(imsg) == "table") then imsg = imsg[1] end
							if (imsg) then						
								local mc = editor_msg_draw_color(imsg)
								t_targeted[it] = esb_makecol(mc)
							elseif (iinst_arch.esb_targ_draw_color) then
								t_targeted[it] = esb_makecol(iinst_arch.esb_targ_draw_color)
							else
								t_targeted[it] = esb_makecol(255,255, 255)
							end
						end
					end
				end
			end
		end 
	end
	
	if (fetches == 0) then
		return 0, nil, nil
	end
	
	for z=1,32767 do
		if (t_targeted[z]) then
			nz = nz + 1
			ltargets[nz] = z
			lmsgs[nz] = t_targeted[z]
		end
	end
	
	if (nz > 0) then
		return nz, lmsgs, ltargets
	else
		return 0, nil, nil
	end
	
end

function ___targ_scanfor(stbl, scani)
	for zi in pairs(stbl) do
		if (stbl[zi] == scani) then
			return true
		end
	end
	return false
end

function __ed_getalltargs_reversed(lev, x, y)
	local nz = 0
	local t_targeted = { }
	local ltargets = { }
	local lmsgs = { }
	
	local obj_here = { }
	local ohnum = 0
	
	 if (lev < 0) then
	 	return 0, nil, nil
	end
	
	for d=0,4 do
		local f = dsb_fetch(lev, x, y, d)
		if (f) then
			for i in pairs(f) do
				local iinst = f[i]
				ohnum = ohnum + 1
				obj_here[ohnum] = iinst				
			end
		end	
	end
	
	if (ohnum == 0) then
		return 0, nil, nil
	end
	
	for tinst in dsb_insts() do	
		if (exvar[tinst] and exvar[tinst].target) then
			local ml, mx, my = dsb_get_coords(tinst)
			if (ml == lev and (mx ~= x or my ~= y)) then
				local imsg = nil
				if (exvar[tinst].msg) then
					imsg = exvar[tinst].msg
				end
				local it = exvar[tinst].target
				local tinst_arch = dsb_find_arch(tinst)
				if (type(it) == "table") then
					for z=1,199 do
						if (it[z]) then
							if (___targ_scanfor(obj_here, it[z])) then
								if (type(imsg) == "table") then
									local mc = editor_msg_draw_color(imsg[z])
									t_targeted[tinst] = esb_makecol(mc)
								elseif (imsg) then
									local mc = editor_msg_draw_color(imsg)
									t_targeted[tinst] = esb_makecol(mc)
								elseif (tinst_arch.esb_targ_draw_color) then
									t_targeted[tinst] = esb_makecol(tinst_arch.esb_targ_draw_color)
								end
							end
						else
						    break
						end     
					end
				else
					if (type(imsg) == "table") then imsg = imsg[1] end
					if (___targ_scanfor(obj_here, it)) then				
						if (imsg) then						
							local mc = editor_msg_draw_color(imsg)
							t_targeted[tinst] = esb_makecol(mc)
						elseif (tinst_arch.esb_targ_draw_color) then
							t_targeted[tinst] = esb_makecol(tinst_arch.esb_targ_draw_color)
						end
					end
				end			
			end
		end
	end
	
	for z=1,32767 do
		if (t_targeted[z]) then
			nz = nz + 1
			ltargets[nz] = z
			lmsgs[nz] = t_targeted[z]
		end
	end
	
	if (nz > 0) then
		return nz, lmsgs, ltargets
	else
		return 0, nil, nil
	end

end

-- To make sure the function is always defined
function editor_msg_draw_color(x)
	return { 192, 192, 192 }
end

function __ed_getalldests(lev, x, y)
	local nz = 0
	local xdest = { }
	local ydest = { }
	
	 if (lev < 0) then
	 	return 0, nil, nil
	end
	
	for d=0,4 do
		local f = dsb_fetch(lev, x, y, d)
		if (f) then
			for i in pairs(f) do
				local iinst = f[i]
				if (exvar[iinst]) then
					local xd = nil
					local yd = nil
					if (not exvar[iinst].lev or
						exvar[iinst].lev == lev) 
					then
						if (exvar[iinst].x) then
							xd = exvar[iinst].x
						end
						if (exvar[iinst].y) then
							yd = exvar[iinst].y
						end
						if (xd or yd) then
							if (not xd) then xd = x end
							if (not yd) then yd = y end
							
							nz = nz + 1
							xdest[nz] = xd
							ydest[nz] = yd
						end
					end
				end
			end
		end 
	end
	
	if (nz > 0) then
		return nz, xdest, ydest
	else
		return 0, nil, nil
	end
	
end

function __ed_item_action_setup(id)
	local target_mode = 0
	local move_mode = 0
	local target_arch = nil
	local msgstr = "M_ACTIVATE"
	local sendmsg = false
	local send_targlist = false
	local ignore_cap = nil
	local t2o = nil
	local inv_target = nil
	local inv_put_away = false
	
	local arch = dsb_find_arch(id)
	
	use_exvar(id)
	
	if (exvar[id].target_mode) then
		target_mode = exvar[id].target_mode
		if (exvar[id].target_arch) then
			target_arch = exvar[id].target_arch
		end
	end

	if (exvar[id].send_msg) then
		local smsg = exvar[id].send_msg
		if (msg_types[smsg]) then
			msgstr = msg_types[smsg]
		end
		sendmsg = true
		if (exvar[id].send_to_target) then
			send_targlist = true
		end
	end
	
	if (exvar[id].move_mode) then
		move_mode = exvar[id].move_mode
		
		if (move_mode == 2 or move_mode == 3) then
			t2o = exvar[id].target_to_operand
			ignore_cap = exvar[id].ignore_capacity
		end
		
		if (move_mode == 4) then
			inv_target = exvar[id].inv_char
			if (exvar[id].inv_put_away) then
				inv_put_away = true
			end
		end		
	end
	
	local mlev, mx, my, mface
	if (exvar[id].move_lev) then mlev = exvar[id].move_lev end
	if (exvar[id].move_x) then mx = exvar[id].move_x end
	if (exvar[id].move_y) then my = exvar[id].move_y end
	if (exvar[id].move_face) then mface = exvar[id].move_face end 	
	
	return target_mode, target_arch, sendmsg, msgstr, send_targlist, move_mode,
		mlev, mx, my, mface, t2o, ignore_cap, inv_target, inv_put_away
	
end

function __ed_item_action_store(id, target_mode, target_arch, sendmsg, msgstr,
	send_targlist, move_mode, mlev, mx, my, mface, t2o, ignore_cap,
	inv_target, inv_put_away)
	
	use_exvar(id)
	exvar[id].target_mode = nil
	exvar[id].target_opby = nil
	exvar[id].target_arch = nil
	if (target_mode >= 3 and not target_arch) then
		target_mode = 0
	end
	if (target_mode >= 1) then
		exvar[id].target_mode = target_mode
	end
	if (target_arch) then
		exvar[id].target_arch = target_arch
	end
	
	exvar[id].send_msg = nil
	exvar[id].send_to_target = nil
	if (sendmsg) then
		exvar[id].send_msg = _G[msgstr]
		if (send_targlist) then
			exvar[id].send_to_target = true
		end
	end
			
	exvar[id].move_mode = nil
	exvar[id].move_lev = nil
	exvar[id].move_x = nil
	exvar[id].move_y = nil
	exvar[id].move_face = nil
	exvar[id].target_to_operand = nil
	exvar[id].ignore_capacity = nil
	exvar[id].inv_char = nil
	exvar[id].inv_put_away = nil
	if (move_mode > 0) then	
		exvar[id].move_mode = move_mode	
		if (move_mode == 1) then
			exvar[id].move_lev = mlev
			exvar[id].move_x = mx
			exvar[id].move_y = my
			exvar[id].move_face = mface
		elseif (move_mode == 2 or move_mode == 3) then
			if (t2o) then
				exvar[id].target_to_operand = true
			end
			if (ignore_cap) then
				exvar[id].ignore_capacity = true
			end
		elseif (move_mode == 4) then
			if (inv_target ~= 0) then
				exvar[id].inv_char = inv_target
				if (inv_put_away) then
					exvar[id].inv_put_away = true
				end
			end
		end	
	end
			
end

function __ed_monstergen_setup(id)
	local arch = dsb_find_arch(id)
	local gen = ""
	local sndval = 1
	local regen = 0
	local hpv = 0
	local perm = { false, false, false, false }
	local moncount = 0
	
	use_exvar(id)
	if (exvar[id].generates) then
		gen = exvar[id].generates
		if (obj[gen]) then
			arch = obj[gen]
		end
	end
	
	if (exvar[id].sound) then
		if (arch.step_sound and exvar[id].sound == arch.step_sound.tblkey) then
			sndval = 2
		else
			sndval = exvar[id].sound
		end
	end
	
	if (exvar[id].regen) then
		regen = exvar[id].regen
	end
	
	if (exvar[id].hp) then
		hpv = exvar[id].hp
	end
	
	if (exvar[id].num_permitted) then
		local gp = exvar[id].num_permitted
		if (type(gp) == "table") then
			perm = { gp[1], gp[2], gp[3], gp[4] }
		end
	elseif (exvar[id].min) then
		perm[exvar[id].min] = true
		if (exvar[id].max and (exvar[id].max > exvar[id].min)) then
			for i=exvar[id].min,exvar[id].max do
				perm[i] = true
			end
		end
	end
	
	if (arch.size) then
		if (arch.size >= 2) then
			perm[3] = 0
			perm[4] = 0
		end
		if (arch.size == 4) then
			perm[2] = 0
		end
	end
	
	local bad = 0
	for chk=1,4 do
		if (perm[chk] ~= true) then
			bad = bad + 1
		end
	end
	if (bad == 4) then
		perm[1] = true
	end
	
	if (exvar[id].count) then 
		moncount = exvar[id].count
	end

	return gen, sndval, perm[1], perm[2], perm[3], perm[4], regen, hpv, moncount	
end

function __ed_monstergen_store(id, gen, snd,
	perm1, perm2, perm3, perm4, regen, hpv, moncount)

	use_exvar(id)
	exvar[id].generates = nil
	exvar[id].num_permitted = nil
	exvar[id].min = nil
	exvar[id].max = nil
	exvar[id].hp = nil
	exvar[id].sound = nil
	exvar[id].regen = nil
	exvar[id].count = nil
	
	if (not gen or not obj[gen]) then return end
	local gen_arch = obj[gen]
	exvar[id].generates = gen	
	
	if (snd == 2) then
		if (gen_arch.step_sound) then
			exvar[id].sound = gen_arch.step_sound.tblkey
		end
	elseif (snd == 1) then
		exvar[id].sound = nil
	elseif (snd) then
		exvar[id].sound = snd
	end
	
	if (regen and regen > 0) then
		exvar[id].regen = regen
	end
	
	if (hpv and hpv > 0) then
		exvar[id].hp = hpv
	end
	
	if (moncount > 0) then
		exvar[id].count = moncount
	end
	
	if (perm1 and not perm2 and not perm3 and not perm4) then
		return
	end
		
	local gpt = { perm1, perm2, perm3, perm4 }
	local mmin = nil
	local mmax = nil
	local gap = false
	local minmax_ok = true
	for i=1,4 do
		local v = gpt[i]
		if (v) then
			if (not mmin) then mmin = i
			elseif (not mmax or i > mmax) then
				if (gap) then
					minmax_ok = false
					break
				end
				mmax = i
			end
		else
			if (mmin) then gap = true end
		end			
	end

	-- Can't find a logical pattern of min/max
	if (not minmax_ok) then	
		exvar[id].num_permitted = gpt
	else
		exvar[id].min = mmin
		exvar[id].max = mmax
	end
end

function __ed_opbyed_store(id, wallitem, tele, location, wallselmode, const_weight, destroy, disable, no_tc, off_trigger,
	opby_party, opby_party_face, wrong_direction_untrigger, opby_thing, opby_monster, opby_arch, opby_arch_class, opby_party_carry, except_when)

	local do_other_opbys = false
	
	use_exvar(id)
	exvar[id].opby = nil
	exvar[id].opby_party = nil
	exvar[id].opby_party_face = nil
	exvar[id].opby_class = nil
	exvar[id].opby_party_carry = nil
	exvar[id].opby_thing = nil
	exvar[id].opby_monster = nil
	exvar[id].opby_suffix = nil
	exvar[id].disable_self = nil
	exvar[id].destroy = nil
	exvar[id].no_tc = nil
	exvar[id].const_weight = nil
	exvar[id].off_trigger = nil
	exvar[id].air = nil
	exvar[id].air_only = nil
	exvar[id].not_in_air = nil
	exvar[id].opby_empty_hand_only = nil
	exvar[id].except_when_carried = nil
	exvar[id].wrong_direction_untrigger = nil
	
	if (location == 3) then
	    exvar[id].air_only = true
	else
		if (tele) then
	    	if (location == 1) then
	        	exvar[id].not_in_air = true
		 	end
		else
		    if (location == 2) then
		        exvar[id].air = true
			end
		end
	end
	
	if (not tele) then
		if (const_weight) then exvar[id].const_weight = true end
		if (destroy) then exvar[id].destroy = true end
		if (disable) then exvar[id].disable_self = true end
		if (no_tc) then exvar[id].no_tc = true end
		if (off_trigger) then exvar[id].off_trigger = true end
	end
	
	if (wallitem) then
	    if (wallselmode == 2) then
	        exvar[id].opby_empty_hand_only = true
		elseif (wallselmode == 3) then
		    do_other_opbys = true
		end
	end
	
	if (not wallitem) then
		if (opby_party) then
		    exvar[id].opby_party = true
		    if (opby_party_face > 0) then
		        exvar[id].opby_party_face = opby_party_face - 1
				if (wrong_direction_untrigger) then
					exvar[id].wrong_direction_untrigger = true
				end
			end
		end
	end
	
	if (opby_thing) then exvar[id].opby_thing = true end
	if (not wallitem and opby_monster) then
	    exvar[id].opby_monster = true
	end
	
	if (not wallitem or do_other_opbys) then
	    if (opby_arch) then
			exvar[id].opby = opby_arch
			if (obj[opby_arch .. "_x"]) then
				exvar[id].opby_suffix = "_x"
			elseif (obj[opby_arch .. "_cursed"]) then
				exvar[id].opby_suffix = "_cursed"
			end
		end
	    if (opby_arch_class) then exvar[id].opby_class = opby_arch_class end
	end
	
	if (not wallitem or wallselmode == 2) then
	    if (opby_party_carry) then exvar[id].opby_party_carry = opby_party_carry end
	end
	
	if (except_when and exvar[id].opby_party_carry) then
		exvar[id].except_when_carried = true
	end

end

function __ed_spined_store(id, sel)
	use_exvar(id)
	exvar[id].spin = nil
	exvar[id].face = nil
	if (sel >= 1 and sel <= 4) then
	    exvar[id].face = sel - 1
	elseif (sel >= 5) then
		exvar[id].spin = sel - 4
	end
	
end

function __ed_shooter_store(id, power, drag, regen, sideinfo, sound, shoots)
	use_exvar(id)
	
	exvar[id].sound = sound
	
	if (shoots == 1) then
	    exvar[id].shoots = nil
	    exvar[id].shoot_square = true
	else
		exvar[id].shoots = shoots
		exvar[id].shoot_square = nil
	end
	
	if (power >= 1) then
		exvar[id].power = power
	else
		exvar[id].power = 1
	end
		
	if (drag >= 1) then
	    exvar[id].delta = drag
	else
	    exvar[id].delta = nil
	end
	    
	if (regen >= 1) then
	    exvar[id].regen = regen
	else
	    exvar[id].regen = nil
	end

    exvar[id].force_side = nil
    exvar[id].double = nil
	if (sideinfo == 3) then
	    exvar[id].double = true
	elseif (sideinfo == 2) then
	    exvar[id].force_side = 0
	elseif (sideinfo == 1) then
	    exvar[id].force_side = 1
	end
	
end

function __ed_doored_store(id, bash_power, fire_power, zo_number)
	use_exvar(id)
	
	if (not zo_number or zo_number == 9999 or zo_number == 0) then
	    exvar[id].zoable = nil
	else
	    exvar[id].zoable = zo_number
	end
	
	local door_arch = dsb_find_arch(id)
	local arch_bash_power = 9999
	local arch_fire_power = 9999
	if (door_arch.bash_power) then
	    arch_bash_power = door_arch.bash_power
	end
	if (door_arch.fire_power) then
		arch_fire_power = door_arch.fire_power
	end
	
	if (arch_bash_power == bash_power) then
	    exvar[id].bash_power = nil
	else
	    exvar[id].bash_power = bash_power
	end
	
	if (arch_fire_power == fire_power) then
	    exvar[id].fire_power = nil
	else
	    exvar[id].fire_power = fire_power
	end

end

function __ed_countered_store(id, count, send_reverse, disable_self)
	use_exvar(id)
	
	if (count >= 0) then
		exvar[id].count = count
	else
		exvar[id].count = 1
	end
	
	if (disable_self) then
		exvar[id].disable_self = true
	else
		exvar[id].disable_self = nil
	end
	
	if (send_reverse) then
		exvar[id].no_reverse = nil
	else
		exvar[id].no_reverse = true
	end

end

function __ed_triggercontroller_getblockdirs_and_bits(id)

	local block_n, block_e, block_s, block_w
	local bit_i_1, bit_i_2, bit_i_3, bit_i_4, bit_t_1, bit_t_2, bit_t_3, bit_t_4  
	local usebits = false
 
	if (not exvar[id] or not exvar[id].block_dirs or
		type(exvar[id].block_dirs) ~= "table") 
	then
		-- nil
	else
		block_n = exvar[id].block_dirs[1]
		block_e = exvar[id].block_dirs[2]
		block_s = exvar[id].block_dirs[3]
		block_w = exvar[id].block_dirs[4]
	end
	
	if (not exvar[id] or not exvar[id].bit_i or
		type(exvar[id].bit_i) ~= "table") 
	then
		-- nil
	else
		usebits = true
		bit_i_1 = exvar[id].bit_i[1]
		bit_i_2 = exvar[id].bit_i[2]
		bit_i_3 = exvar[id].bit_i[3]
		bit_i_4 = exvar[id].bit_i[4]
	end
	
	if (not exvar[id] or not exvar[id].bit_t or
		type(exvar[id].bit_t) ~= "table") 
	then
		-- nil
	else
		usebits = true
		bit_t_1 = exvar[id].bit_t[1]
		bit_t_2 = exvar[id].bit_t[2]
		bit_t_3 = exvar[id].bit_t[3]
		bit_t_4 = exvar[id].bit_t[4]
	end
	
	return block_n, block_e, block_s, block_w, usebits, bit_i_1, bit_i_2, bit_i_3, bit_i_4, bit_t_1, bit_t_2, bit_t_3, bit_t_4 
end

function __ed_triggercontroller_store(id, tp_options, tp_members, pr, send_reverse, send_reverse_only, orig_preserve, disable_self,
	block_n, block_e, block_s, block_w,
	using_bits, bit_i_1, bit_i_2, bit_i_3, bit_i_4, bit_t_1, bit_t_2, bit_t_3, bit_t_4 )

	use_exvar(id)
	if (pr > 0 and pr < 100) then
		exvar[id].probability = pr
	else
		exvar[id].probability = nil
	end
	
	exvar[id].send_reverse_only = nil
	if (send_reverse) then
	    exvar[id].send_reverse = true
		if (send_reverse_only) then
			exvar[id].send_reverse_only = true
		end
	else
	    exvar[id].send_reverse = nil
	end
	
	if (orig_preserve) then
	    exvar[id].originator_preserve = true
	else
	    exvar[id].originator_preserve = nil
	end
	
	if (disable_self) then
		exvar[id].disable_self = true
	else
		exvar[id].disable_self = nil
	end
	
	exvar[id].party_contains = tp_options
	if (tp_members > 1) then
	    exvar[id].party_members = tp_members
	else
	    exvar[id].party_members = nil
	end
	
	if (block_n or block_e or block_s or block_w) then
		exvar[id].block_dirs = { block_n, block_e, block_s, block_w }
	else
		exvar[id].block_dirs = nil
	end
	
	if (using_bits) then
		exvar[id].bit_i = { bit_i_1, bit_i_2, bit_i_3, bit_i_4 }
		exvar[id].bit_t = { bit_t_1, bit_t_2, bit_t_3, bit_t_4 }
	else
		exvar[id].bit_i = nil
		exvar[id].bit_t = nil
	end
	
end

function __ed_swap_checkbox(id)
	if (exvar[id] and exvar[id].full_swap == true) then
	    return 1
	end
	return nil
end

function __ed_swap_store(id, arch, full, target_opby)
	use_exvar(id)
	if (full) then
	    exvar[id].full_swap = true
	else
	    exvar[id].full_swap = nil
	end
	
	if (target_opby) then
	    exvar[id].target_opby = true
	    exvar[id].target = nil
	else
	    exvar[id].target_opby = nil
	end

	exvar[id].arch = arch

end

function __ed_repeat_store(id, rep)
	use_exvar(id)
	if (rep > 0) then
		exvar[id].repeat_rate = rep
	else
		exvar[id].repeat_rate = nil
	end
end

function __ed_probability_store(id, pr)
	use_exvar(id)
	if (pr > 0 and pr < 100) then
		exvar[id].probability = pr
	else
		exvar[id].probability = nil
	end
end

function __ed_mirrorchampion_store(id, who, resrei)
	use_exvar(id)
	if (who == 0) then
	    exvar[id].champion = nil
	else
	    exvar[id].champion = who
	end
	
	if (resrei == 3) then
	    exvar[id].offer_mode = nil
	else
	    exvar[id].offer_mode = resrei
	end
end

function __ed_func_caller_store(id, fm_a, fm_d, fm_t, fm_n)
	use_exvar(id)
	exvar[id].m_a = fm_a
	exvar[id].m_d = fm_d
	exvar[id].m_t = fm_t
	exvar[id].m_n = fm_n
end

function __ed_champ_create(id, designation)
	dsb_add_champion(id, designation, "port_mophus", string.upper(designation), "", 1000, 1000, 1000, 400, 400, 400, 400, 400, 400, 400, 0, 0, 0, 0)
end

function editor_exvar_coordinate_picker(id, lev, x, y)
	local loc_lev, loc_x, loc_y = dsb_get_coords(id)
	use_exvar(id)
	if (loc_lev == lev) then exvar[id].lev = nil
	else exvar[id].lev = lev end
	if (loc_x == x and loc_y == y) then
	    exvar[id].x = nil
	    exvar[id].y = nil
	else
		exvar[id].x = x
		exvar[id].y = y
	end
end

function editor_exvar_target_picker(id, tmp_table)
	local arch = dsb_find_arch(id)
	local highest = 0
	use_exvar(id)
	exvar[id].target = nil
	exvar[id].msg = nil
	exvar[id].delay = nil
	
	local ttbl = { }
	for z=1,99 do
		if (not tmp_table[z]) then break end
		ttbl[z] = tmp_table[z][1]
		highest = z
	end	
	
	if (highest == 0) then
		return
	end
		
	if (ttbl[2]) then
		exvar[id].target = ttbl
	elseif (ttbl[1]) then
		exvar[id].target = ttbl[1]
	end
	
	if (exvar[id].target) then
	    exvar[id].target_opby = nil
	end
	
    if (arch.esb_special_trigger) then
    	return
    end
    
    local msg_same = nil
    local delay_same = nil
	local data_same = nil
    local mtbl = { }
    local dtbl = { }
	local ttbl = { }
    
    for z=1,highest do
    	mtbl[z] = tmp_table[z][2]
		dtbl[z] = tmp_table[z][3]
		ttbl[z] = tmp_table[z][4]
		
		if (msg_same == nil) then
			msg_same = mtbl[z]
		elseif (msg_same ~= mtbl[z]) then
			msg_same = false
		end 
		
		if (delay_same == nil) then
			delay_same = dtbl[z]
		elseif (delay_same ~= dtbl[z]) then
			delay_same = false
		end
		
		if (data_same == nil) then
			data_same = ttbl[z]
		elseif (data_same ~= ttbl[z]) then
			data_same = false
		end
		
	end
	
	if (msg_same) then
		exvar[id].msg = msg_same
	else
		exvar[id].msg = mtbl
	end
	
	if (data_same ~= false) then
		exvar[id].data = data_same
	else
		exvar[id].data = ttbl
	end
	
	if (delay_same) then
		if (delay_same > 0) then
			exvar[id].delay = delay_same
		end
	elseif (delay_same == nil) then
		exvar[id].delay = nil
	else
		exvar[id].delay = dtbl
	end
	
end

function editor_get_sound_options(id, tele)
	local arch = dsb_find_arch(id)
	local def = false
	local dsel = 1
	
	if (arch.esb_special_trigger) then
		return true, false, false, 1	
	elseif (arch.esb_always_silent) then
		return true, false, false, 1
	end
	
	use_exvar(id)
	if (exvar[id].sound) then
		dsel = 3
	end
	
	if (not arch.default_silent) then
		if (arch.class == "TELEPORTER") then
			def = true
		elseif (arch.type == "WALLITEM") then
			def = true
		elseif (arch.class == "TRIGGER") then
			def = true
		elseif (arch.class == "DOORBUTTON") then
		    def = true
		elseif (arch.default_sound) then
			def = true
		end
		
		if (def == true and dsel == 1) then
			dsel = 2
		end
	end
	
	if (exvar[id].silent) then
		dsel = 1
	end

	return true, def, true, dsel
end

function editor_store_sound_exvars(id, tele, silent, def, use_sound, sound_name)
	local arch = dsb_find_arch(id)
	use_exvar(id)
	if (not silent) then
		exvar[id].silent = nil
	else
		if (not arch.default_silent and not arch.esb_special_trigger
			and not arch.esb_always_silent) 
		then
			exvar[id].silent = true
		end
	end
	if (use_sound) then
		exvar[id].sound = sound_name
	else
		exvar[id].sound = nil
	end
end 

function __ed_find_insts(archname, opby)
	local objarch = obj[archname]
	local res = 0
	
	for i in dsb_insts() do
	    local iarch = dsb_find_arch(i)
	    local match = false
	    if (opby) then
			if (exvar[i] and exvar[i][opby]) then
				if (type(exvar[i][opby]) == "string") then
				    if (obj[exvar[i][opby]] == objarch) then
				        match = true
					end
				elseif (type(exvar[i][opby]) == "table") then
					for oobj in pairs(exvar[i][opby]) do
						if (obj[exvar[i][opby][oobj]] == objarch) then
						    match = true
						    break
						end
					end
				end
			end
		else
			if (objarch == iarch) then
				match = true
			end
		end
	    if (match) then
	        local itag = i
	        local lev, x, y, face = dsb_get_coords(i)
	        if (opby) then
	            local iarch = dsb_find_arch(i)
	            itag = i .. " (" .. iarch.ARCH_NAME .. ")"
			end
	        res = res + 1
			if (lev >= 0) then
			    local cell = dsb_get_cell(lev, x, y)
				local facestr = "C"
				if (cell) then
				    if (face == NORTH) then facestr = "N" end
				    if (face == SOUTH) then facestr = "S" end
				    if (face == EAST) then facestr = "E" end
				    if (face == WEST) then facestr = "W" end
				else
				    if (face == NORTHEAST) then facestr = "NE" end
				    if (face == NORTHWEST) then facestr = "NW" end
				    if (face == SOUTHEAST) then facestr = "SE" end
				    if (face == SOUTHWEST) then facestr = "SW" end
				end

	        	__l_tag(itag .. " @ " .. lev .. ", " .. x .. ", " .. y .. ", " .. facestr)
			elseif (lev == IN_OBJ) then
			    local xarch = dsb_find_arch(x)
			    local xarchname = xarch.ARCH_NAME
				__l_tag(itag .. " @ Inside " .. x .. " (" .. xarchname .. ")")
			elseif (lev == CHARACTER) then
			    local champname = reverse_champion_table[x]
			    local slotid = y
			    if (inventory_info[y] and inventory_info[y].name) then
			    	slotid = y .. "/" .. inventory_info[y].name
				end
			    if (champname == nil) then
			        champname = "[CHAR " .. x .. "]"
				end
			    __l_tag(itag .. " @ Char " .. champname .. " <" .. slotid .. ">")
			else
				__l_tag(itag .. " @ Limbo")
			end
		end
	    
	
	end
	
	return res
end

function dsb_insts()
	return __nextinst, 0, 0
end

function ___nilreturner()
	return nil
end

function ___inside_iterator(tstate, n)
	local nkey = tstate.pfunc(tstate.pstate, tstate.nkey)
	if (nkey == nil) then return nil end
	tstate.nkey = nkey
	return tstate.tab[nkey]
end

function __handle_arch_messages(arch, tablestring, id, msgtype, data, sender)
	if (arch[tablestring]) then
		if (type(arch[tablestring]) == "function") then
			arch[tablestring](id, msgtype, data, sender)
			return true
		else
			if (type(arch[tablestring][msgtype]) == "function") then
				arch[tablestring][msgtype](id, data, sender)
				return true
			elseif (type(arch[tablestring][msgtype]) == "string") then
				local cfunc = dsb_lookup_global(arch[tablestring][msgtype])
				cfunc(id, data, sender)
				return true
			elseif (arch[tablestring][msgtype] == false) then
				return true
			end
		end
	end
	
	return false
end

function __main_msg_processor(id, id_luaname, msgtype, data, sender)
	local arch = obj[id_luaname]

	local mh = __handle_arch_messages(arch, "msg_handler", id, msgtype, data, sender)
	local mhx = __handle_arch_messages(arch, "msg_handler_ext", id, msgtype, data, sender)
	
	if (not mh and not mhx) then
		if (sys_default_msg_handler) then
			if (type(sys_default_msg_handler) == "function") then
				sys_default_msg_handler(id, msgtype, data, sender)
			else
				if (type(sys_default_msg_handler[msgtype]) == "function") then
					sys_default_msg_handler[msgtype](id, data, sender)
				elseif (type(sys_default_msg_handler[msgtype]) == "string") then
					local cfunc = dsb_lookup_global(sys_default_msg_handler[msgtype])
					cfunc(id, data, sender)
				end
			end
		end
	end
	
end

function dsb_in_obj(id)
	local inl = dsb_fetch(IN_OBJ, id, -1, 0)
	if (not inl) then
		return ___nilreturner, nil, nil
	end
	local pfunc, pstate, ix = pairs(inl)
	tstate = { tab = inl, pfunc = pfunc, pstate = pstate, nkey = ix }
	return ___inside_iterator, tstate, ix  
end

function dsb_add_msgtype(name, id)
	if (not msg_types) then msg_types = { } end
	if (type(name) == "string") then
		_G[name] = id
		msg_types[id] = name
	end
end

function dsb_get_light(handle)
	if (type(__DSB_LIGHT_HANDLES) ~= "table") then
	    __DSB_LIGHT_HANDLES = { }
	end
	local h_type = type(handle)
	if (h_type == "string" or h_type == "number") then
	    if (__DSB_LIGHT_HANDLES[handle]) then
	        return __DSB_LIGHT_HANDLES[handle]
		else
		    return 0
		end
	end
end

function dsb_set_light(handle, lvalue)
	if (type(__DSB_LIGHT_HANDLES) ~= "table") then
	    __DSB_LIGHT_HANDLES = { }
	end
	local h_type = type(handle)
	if (h_type == "string" or h_type == "number") then
	    if (lvalue < -300000) then lvalue = -300000 end
	    if (lvalue > 300000) then lvalue = 300000 end
	    lvalue = math.floor(lvalue)
	    
		__DSB_LIGHT_HANDLES[handle] = lvalue
		
		local total_light = 0
		for i in pairs(__DSB_LIGHT_HANDLES) do
		    local v = __DSB_LIGHT_HANDLES[i]
		    total_light = total_light + v
		end
		
		__set_light_raw(0, total_light)
	end
end
dsb_export("__DSB_LIGHT_HANDLES")

function __ed_copy_begin()
	__EDITOR_COPY_TABLE = { }
end

function __ed_copy_notify(from, to)
	__EDITOR_COPY_TABLE[from] = to
end

function __ed_copy_finish()
	for c in pairs(__EDITOR_COPY_TABLE) do
	    local id_from = c
	    local id_to = __EDITOR_COPY_TABLE[c]
	
		if (exvar[id_from] and type(exvar[id_from]) == "table") then
			exvar[id_to] = { }
			for cxv in pairs(exvar[id_from]) do
				if (type(exvar[id_from][cxv]) == "table") then
				    exvar[id_to][cxv] = { }
				    for cxvt in pairs(exvar[id_from][cxv]) do
				        exvar[id_to][cxv][cxvt] = exvar[id_from][cxv][cxvt]
				    end
				else
					exvar[id_to][cxv] = exvar[id_from][cxv]
				end
		    end
		    
		    if (exvar[id_to].target) then
		        local targ = exvar[id_to].target
		        if (type(targ) == "table") then
					for it in pairs(targ) do
					    if (__EDITOR_COPY_TABLE[targ[it]]) then
							exvar[id_to].target[it] = __EDITOR_COPY_TABLE[targ[it]]
						end
					end
		        else
		            if (__EDITOR_COPY_TABLE[targ]) then
						exvar[id_to].target = __EDITOR_COPY_TABLE[targ]
					end
				end
			end
			
		end
	end

end

function __enter_champion_preview(who)
	for sl=0,(MAX_INV_SLOTS-1) do -- Start counting from 0
		local f = dsb_fetch(CHARACTER, who, sl, 0)
		if (f) then
			use_exvar(f)
			exvar[f].GO_IN_SLOT = sl
		end
	end
end

function __exit_champion_preview(who, i_taken)
	for i in dsb_insts() do
		if (exvar[i] and exvar[i].GO_IN_SLOT) then
			local f = i
			
			if (i_taken == 0) then
				local targsl = exvar[f].GO_IN_SLOT
				local targgot = dsb_fetch(CHARACTER, who, targsl, 0)
				if (targgot) then
					dsb_move(targgot, LIMBO, 0, 0, 0)
				end
				dsb_move(f, CHARACTER, who, targsl, 0)
			end
			exvar[f].GO_IN_SLOT = nil
		end
	end

end

function __rebuild_exported_characters(p1, p2, p3, p4)
	__NEW_CHAMPIONS = { }
	local cht = {p1, p2, p3, p4}
	for pp=1,4 do
		if (cht[pp]) then
			dsb_champion_fromparty(pp-1)
			
			local ctbl = cht[pp]
			local lastitem = #ctbl
			local unarmed_method_name = ctbl[lastitem]
			ctbl[lastitem] = nil

			local newguy = dsb_add_champion(unpack(ctbl))
			if (unarmed_method_name ~= "unarmed_methods") then
				dsb_replace_methods(newguy, unarmed_method_name)
			end
			dsb_champion_toparty(pp-1, newguy)
			
			__NEW_CHAMPIONS[pp] = newguy		
		end
	end
end
