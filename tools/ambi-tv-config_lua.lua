-- Ambi-TV-Steuerung
-- Idee: Tischi
-- Realisierung: SnowHead
-- Lizenz: GPL 2
-- Version: 1.3
-- Stand: 30.09.2015

local json = require "json"
local posix	= require "posix"
n = neutrino()
confFile = "/var/tuxbox/config/ambi-tv.conf"
config = configfile.new()
configChanged = 0
conf={}
conf["RaspyIP"]="192.168.178.53"
conf["Port"]=16384
conf["brightness"]=100
conf["intensity-red"]=100
conf["intensity-green"]=100
conf["intensity-blue"]=100
conf["intensity-min-red"]=3
conf["intensity-min-green"]=3
conf["intensity-min-blue"]=3
conf["gamma-red"]=1.55
conf["gamma-green"]=1.6
conf["gamma-blue"]=1.5
conf["sensitivity"]=100
conf["linear"]="aus"
conf["falloff"]="ein"
conf["average"]="ein"
conf["integral"]="ein"
ambi_conf="/tmp/ambiconf.txt"

function ask_config(v, d)
	rval = d
	fnam = os.tmpname()
	os.execute("wget -q -Y off -O " .. fnam .. " " .. conf["RaspyIP"] .. ":" .. conf["Port"] .. "?" .. v .. "=")
	tf = io.open(fnam, "r")
	tval = tf:read("*n")
	if tval ~= nil then
		rval = tval
	end
	tf:close()
	os.remove(fnam)
	return rval
end

function read_config(info)
	if info then
		h= hintbox.new{ title="Ambi-TV", text="aktuelle Einstellungen werden abgefragt ...", icon="info"}
		h:paint()
	end
	
	conf["brightness"] = ask_config("overall-brightness", 100)
	conf["intensity-red"] = ask_config("intensity-red", 100)
	conf["intensity-green"] = ask_config("intensity-green", 100)
	conf["intensity-blue"] = ask_config("intensity-blue", 100)
	conf["intensity-min-red"] = ask_config("intensity-min-red", 3)
	conf["intensity-min-green"] = ask_config("intensity-min-green", 3)
	conf["intensity-min-blue"] = ask_config("intensity-min-blue", 3)
	conf["gamma-red"] = ask_config("gamma-red", 1.55)
	conf["gamma-green"] = ask_config("gamma-green", 1.6)
	conf["gamma-blue"] = ask_config("gamma-blue", 1.5)
	conf["sensitivity"] = ask_config("sensitivity", 100)
	smoth_ges = ask_config("smoothing", 7)
	sm = smoth_ges
	if sm >= 4 then
		conf["falloff"] = "ein"
		sm = sm - 4
	else
		conf["falloff"] = "aus"
	end
	if sm >= 2 then
		conf["average"] = "ein"
		sm = sm - 2
	else
		conf["average"] = "aus"
	end
	if sm == 1 then
		conf["integral"] = "ein"
	else
		conf["integral"] = "aus"
	end
	sm = ask_config("linear", 0)
	if sm == 1 then
		conf["linear"] = "ein"
	else
		conf["linear"] = "aus"
	end
	
	if info then
		h:hide()
	end
end


--Funktion zum laden der Config
function loadConfig()
	config:loadConfig(confFile)
	conf["RaspyIP"] = config:getString("RaspyIP", "192.168.178.53")
	conf["Port"] = config:getString("Port", 16384)
end

--Funktion zum speichern der Config
function saveConfig()
	if configChanged == 1 then
		config:setString("RaspyIP", conf["RaspyIP"])
		config:setString("Port", conf["Port"])
		config:saveConfig(confFile)
		configChanged = 0
		posix.sleep(1)
	end
end

function ambi_modus(v)
	os.execute("wget -q -Y off -O /dev/null " .. conf["RaspyIP"] .. ":" .. conf["Port"] .. "?mode=" .. v)
	read_config(false)
end

function set_var(k,v)
	conf[k] = v
	os.execute("wget -q -Y off -O /dev/null " .. conf["RaspyIP"] .. ":" .. conf["Port"] .. "?" .. k .. "=" .. v)
end

function set_var2(k,v)
	configChanged = 1
	conf[k] = v
end

function set_var3(k,v)
	conf[k]=v
	if conf["falloff"] == "ein" then
		smoth_ges = 1 else smoth_ges = 0
	end
	if conf["average"] == "ein" then
		smoth_ges = smoth_ges + 2
	end
	if conf["integral"] == "ein" then
		smoth_ges = smoth_ges + 4
	end
end

function set_var4(k,v)
	wd = 0
	conf[k]=v
	if v == "ein" then
		wd = 1
	end
	os.execute("wget -q -Y off -O /dev/null " .. conf["RaspyIP"] .. ":" .. conf["Port"] .. "?" .. k .. "=" .. wd)
end

function smoothing_send()
	os.execute("wget -q -Y off -O /dev/null " .. conf["RaspyIP"] .. ":" .. conf["Port"] .. "?smoothing=" .. smoth_ges)
end

--Menü anzeigen
function addMenue()
	loop = true
	line = nil
	count = 1
	
	os.execute("wget -q -Y off -O- " .. conf["RaspyIP"] .. ":" .. conf["Port"] .. "?getconfig | sed -n /^\\&/p > " .. ambi_conf)

	m = menu.new{name="Ambi-TV Steuerung", has_shadow=true}

	f = io.open(ambi_conf, "r")
	line = f:read("*l")
	if line == nil then
		m:addItem{type="separatorline", name="Keine Verbindung zu Ambi-TV!"}
		m:addItem{type="separatorline", name="Netzwerkeinstellung überprüfen!"}
	else
		m:addItem{type="separatorline", name="Ambi-TV Modus"}
		read_config(true)
	    while loop do
			if line ~= nil then
				spos = n:strFind(line, "#")
				if spos ~= nil then
					mode = n:strSub(line, spos + 1)
				else
					spos = n:strFind(line, "{")
					spos2 = n:strFind(line, " ")
					if spos2 < spos then
						spos = spos2
					end
					mode = n:strSub(line, 1, spos - 1)
				end
				mode = mode:gsub("^%s*(.-)%s*$", "%1")
				line = f:read("*l")
				
				if count < 10 then
					m:addItem{type="forwarder", action="ambi_modus", name=mode, id=count - 1 , icon=count, directkey=RC[string.format("%d",count)]}
				else
					m:addItem{type="forwarder", action="ambi_modus", name=mode, id=count - 1}
				end
				count = count + 1
			else
				loop = false
			end

	    end
		m:addItem{type="forwarder", action="ambi_modus", name="Ausschalten", id=count - 1 , icon="0", directkey=RC["0"]}

		m:addItem{type="separatorline", name="Ambi-TV Parameter"}
		m:addItem{type="stringinput", action="set_var", id="brightness", value=conf["brightness"],  valid_chars="0123456789", size=3, name="Helligkeit einstellen in %", icon="rot", directkey=RC["red"]}
		m:addItem{type="forwarder", action="color_menu", name="Farbeinstellungen", icon="gruen", directkey=RC["green"]}
		m:addItem{type="forwarder", action="audio_menu", name="Audioeinstellungen", icon="gelb", directkey=RC["yellow"]}
	end

	m:addItem{type="forwarder", action="network_menu", name="Netzwerkeinstellungen", icon="blau", directkey=RC["blue"]}
	m:exec()
	
	if configChanged then
		saveConfig()
	end
end

function color_menu()
	m:hide()
	l = menu.new{name="Farbeinstellungen", has_shadow=true}
	l:addItem{type = "back"}
	l:addItem{type="separatorline"}
	l:addItem{type="stringinput", action="set_var", id="intensity-red", value=conf["intensity-red"],  valid_chars="0123456789", size=3, name="Intensität Rot in %", icon="rot", directkey=RC["red"]}
	l:addItem{type="stringinput", action="set_var", id="intensity-green", value=conf["intensity-green"],  valid_chars="0123456789", size=3, name="Intensität Grün in %", icon="gruen", directkey=RC["green"]}
	l:addItem{type="stringinput", action="set_var", id="intensity-blue", value=conf["intensity-blue"],  valid_chars="0123456789", size=3, name="Intensität Blau in %", icon="gelb", directkey=RC["yellow"]}
	l:addItem{type="separatorline"}
	l:addItem{type="stringinput", action="set_var", id="intensity-min-red", value=conf["intensity-min-red"],  valid_chars="0123456789", size=3, name="Minimalintensität Rot in %", icon="blau", directkey=RC["blue"]}
	l:addItem{type="stringinput", action="set_var", id="intensity-min-green", value=conf["intensity-min-green"],  valid_chars="0123456789", size=3, name="Minimalintensität Grün in %", icon="1", directkey=RC["1"]}
	l:addItem{type="stringinput", action="set_var", id="intensity-min-blue", value=conf["intensity-min-blue"],  valid_chars="0123456789", size=3, name="Minimalintensität Blau in %", icon="2", directkey=RC["2"]}
	l:addItem{type="separatorline"}
	l:addItem{type="stringinput", action="set_var", id="gamma-red", value=conf["gamma-red"],  valid_chars="0123456789.", size=4, name="Gammawert Rot", icon="3", directkey=RC["3"]}
	l:addItem{type="stringinput", action="set_var", id="gamma-green", value=conf["gamma-green"],  valid_chars="0123456789.",size=4, name="Gammawert Grün", icon="4", directkey=RC["4"]}
	l:addItem{type="stringinput", action="set_var", id="gamma-blue", value=conf["gamma-blue"], valid_chars="0123456789.", size=4, name="Gammawert Blau", icon="5", directkey=RC["5"]}
	l:exec()
end

function audio_menu()
	m:hide()
	a = menu.new{name="Audioeinstellungen", has_shadow=true}
	a:addItem{type = "back"}
	a:addItem{type="stringinput", action="set_var", id="sensitivity", value=conf["sensitivity"],  valid_chars="0123456789", size=4, name="Empfindlichkeit", icon="rot", directkey=RC["red"]}
	a:addItem{type="forwarder", action="smoothing_menu", name="Smoothing", icon="gruen", directkey=RC["green"]}
	a:addItem{type="chooser", action="set_var4", options={ "ein", "aus" }, id="linear", value=conf["linear"], name="Linear", icon="gelb", directkey=RC["yellow"]}
	a:exec()
end

function smoothing_menu()
	m:hide()
	s = menu.new{name="Smoothing", has_shadow=true}
	s:addItem{type = "back"}
	s:addItem{type="chooser", action="set_var3", options={ "ein","aus" }, id="falloff", value=conf["falloff"], name="Falloff", icon="rot", directkey=RC["red"]}
	s:addItem{type="chooser", action="set_var3", options={ "ein","aus" }, id="average", value=conf["average"], name="Average", icon="gruen", directkey=RC["green"]}
	s:addItem{type="chooser", action="set_var3", options={ "ein","aus" }, id="integral", value=conf["integral"], name="Integral", icon="gelb", directkey=RC["yellow"]}
	s:exec()
	smoothing_send()
end

function network_menu()
	m:hide()
	ne = menu.new{name="Netzwerkeinstellungen", has_shadow=true}
	ne:addItem{type="separatorline", name="Ambi-TV IP/Port"}
	ne:addItem{type = "back"}
	ne:addItem{type="stringinput", action="set_var2", id="RaspyIP", value=conf["RaspyIP"],  sms=1, name="IP/Hostname", icon="rot", directkey=RC["red"]}
	ne:addItem{type="stringinput", action="set_var2", id="Port", value=conf["Port"], valid_chars="0123456789", size=5, name="Port (Standard 16384)", icon="gruen", directkey=RC["green"]}
	ne:exec()	
end

loadConfig()
addMenue()
