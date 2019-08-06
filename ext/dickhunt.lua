privmsg = require("privmsg")
dickgen = require("dickgen")
ualias = require("ualias")
osr = require("osrandom")

driver = require("luasql.postgres")
env = driver.postgres()
db = env:connect("ircbot", "ircbot", "canttouchthis", "192.168.90.255")

db:execute("CREATE TABLE IF NOT EXISTS irc_hunts (channel varchar(256) NOT NULL, network varchar(64) NOT NULL, state boolean, sent boolean, lastact timestamp, score integer, UNIQUE(channel, network))")
db:execute("CREATE TABLE IF NOT EXISTS irc_hunters (nick varchar(256) NOT NULL, channel varchar(256) NOT NULL, network varchar(64) NOT NULL,  goodact integer, badact integer, UNIQUE (nick, channel, network))")
db:execute("CREATE TABLE IF NOT EXISTS irc_hunter_times (nick varchar(256) NOT NULL, channel varchar(256) NOT NULL, network varchar(64) NOT NULL, t double precision)")

-- { { frequency, string }, ... }
-- Frequency from 1 to 10
-- string should contain
-- %NICK%, %CHAN%, %TIME% and %TOT%
-- to be replaced by the data
succfmt = {
	{ 10, "%NICK% you've sucked that dick in %TIME% seconds! You've sucked a total of %TOT% dicks in %CHAN%" },
	{ 1, "%NICK% you've sucked that THICC DICC in %TIME% seconds! You've sucked a total of %TOT% THICC DICCs in %CHAN%" },
	{ 1, "Licking some of the remaining cum from their lips, %NICK% suddenly realised how gay they've become. They've sucked this one in %TIME% seconds, for a new total of %TOT% dicks in %CHAN%" },
	{ 1, "%NICK% the taste of cum is the taste of victory. You've achieved it in %TIME% seconds. You've sucked a total of %TOT% dicks in %CHAN%" } 
}

cuttfmt = {
	{ 10, "%NICK% you've cut that dick in %TIME% seconds! You've cut a total of %TOT% dicks in %CHAN%" },
	{ 1, "%NICK% you've cut that THICC DICC in %TIME% seconds! You've cut a total of %TOT% THICC DICCs in %CHAN%" },
	{ 1, "%NICK% you've cut this dick open in %TIME% and devoured its insides to gain its powers. It didn't work. You've cut a total of %TOT% dicks in %CHAN%" },
	{ 1, "%NICK% after a powerful swing, you find it lying on the ground in pain. You walk away as it suffers. You cut that dick in %TIME% seconds, for a new total of %TOT% dicks in %CHAN%" }
}

function check_state(chan, net)
	local res = {}
	local cur = db:execute("SELECT state FROM irc_hunts WHERE channel = '"..db:escape(chan).."' AND network = '"..db:escape(net).."' LIMIT 1")
	if cur ~= nil then
		cur:fetch(res)
	else
		return false
	end

	return (res[1] == 't')
end

function start()
	local chan = irc.params(1)
	local net = irc.network()

	if check_state(chan, net) == true then
		privmsg.reply(irc, "There's already a hunt in "..chan.."!")
		return
	end

	if string.match(chan, "^#") ~= nil then
		privmsg.reply(irc, "You're not getting a personal dickhunt, you pervert!")
		return
	end

	start_hunt(chan, net)
end

function stop()
	local chan = irc.params(1)
	local net = irc.network()

	if check_state(chan, net) == false then
		privmsg.reply(irc, "There's no hunt going on in "..chan..".")
		return
	end

	stop_hunt(chan, net)
end

function start_hunt(chan, net)
	local echan = "'"..db:escape(chan).."'"
	local enet = "'"..db:escape(net).."'"
	local inchan = " WHERE CHANNEL = "..echan.." AND NETWORK = "..enet
	local res = {}
	local cur = db:execute("SELECT count(*) FROM irc_hunts"..inchan)
	if cur ~= nil then
		cur:fetch(res)
	end

	if res[1] ~= nil and tonumber(res[1]) > 0 then
		db:execute("UPDATE irc_hunts SET state = 't', sent = 'f', score = "..sender.init_score()..inchan)
	else
		db:execute("INSERT INTO irc_hunts (channel, network, state, sent, lastact, score) VALUES ("..echan..", "..enet..", 'f', 'f', now(), 0)")
	end

	privmsg.reply(irc, "Hunt started in "..chan.."! Wait for a dick to show up, then use %succ to suck it, or %cutt to cut it. Do this quick! If they get in your arse they're not getting out.")
end

function stop_hunt(chan, net)
	db:execute("UPDATE irc_hunts SET state = 'f', sent = 'f', score = 0 WHERE channel = '"..db:escape(chan).."' AND network = '"..db:escape(net).."'")
	privmsg.reply(irc, "The hunt in "..chan.." was stopped.")
end

function tick()
	chan = "'"..db:escape(irc.params(1)).."'"
	net = "'"..db:escape(irc.network()).."'"
	db:execute("UPDATE irc_hunts SET score = score + 1 WHERE channel = "..chan.." AND network = "..net)
end

function handle_msg()
	tick()

	local text = irc.tail()
	if text ~= nil and string.len(text) < 4 then
		return
	end

	local cmd = string.sub(string.lower(text), 1, 4)
	if cmd ~= "succ" and cmd ~= "cutt" then
		return
	end

	local nick = ualias.main_alias(net, privmsg.get_nick(irc))

	local enick = "'"..db:escape(nick).."'"
	local chan = irc.params(1)
	local echan = "'"..db:escape(chan).."'"
	local net = irc.network()
	local enet = "'"..db:escape(net).."'"

	-- restrict query to the current channel
	local inchan = " WHERE channel = "..echan.." AND network = "..enet
	-- restrict query to the sender from the current channel
	local fornick = inchan.." AND lower(nick) = lower("..enick..")"

	local dbact = ""
	if cmd == "succ" then
		dbact = "goodact"
	else
		dbact = "badact"
	end

	local chaninfo = {}
	local cur = db:execute("SELECT state, sent, EXTRACT(EPOCH FROM AGE(now(), lastact)) FROM irc_hunts"..inchan)
	if cur ~= nil then
		cur:fetch(chaninfo)
	end
	if chaninfo[1] ~= 't' then
		privmsg.reply_notice(irc, "There is no hunt going on in this channel")
		return
	end

	if chaninfo[2] ~= 't' then
		privmsg.reply_notice(irc, "There's no dick, what are you trying to "..cmd.."?")
		return
	end

	db:execute("UPDATE irc_hunts SET sent = 'f', lastact = now(), score = "..sender.init_score()..inchan)

	local nickinfo = {}
	cur = db:execute("SELECT nick, goodact, badact FROM irc_hunters"..fornick)
	if cur ~= nil then
		cur:fetch(nickinfo)
	end
	if nickinfo[1] == nil then
		nickinfo[1] = nick
		if cmd == "succ" then
			nickinfo[2] = 1
			nickinfo[3] = 0
		else
			nickinfo[2] = 0
			nickinfo[3] = 1
		end

		db:execute("INSERT INTO irc_hunters VALUES ("..enick..", "..echan..", "..enet..", "..nickinfo[2]..", "..nickinfo[3]..")")
	else
		if cmd == "succ" then
			nickinfo[2] = nickinfo[2] + 1
		else
			nickinfo[3] = nickinfo[3] + 1
		end

		db:execute("UPDATE irc_hunters SET "..dbact.." = "..dbact.." + 1"..fornick)
	end

	db:execute("INSERT INTO irc_hunter_times VALUES ("..enick..", "..echan..", "..enet..", "..chaninfo[3]..")")

	nickinfo.nick = nickinfo[1]
	nickinfo.chan = chan
	nickinfo.net = net
	nickinfo.succ = nickinfo[2]
	nickinfo.cutt = nickinfo[3]
	nickinfo.time = chaninfo[3]
	nickinfo.act = cmd
	if cmd == "succ" then
		nickinfo.tot = nickinfo.succ
	else
		nickinfo.tot = nickinfo.cutt
	end

	privmsg.reply(irc, format_act(nickinfo))
end

function format_act(nickinfo)
	local restab, s, tot
	if nickinfo.act == "succ" then
		restab = succfmt
	else
		restab = cuttfmt
	end

	local n = 0
	for i, t in pairs(restab) do
		n = n + t[1]
	end

	math.randomseed(osr.random())
	local index = math.random(1, n)
	n = 0
	for i, t in pairs(restab) do
		n = n + t[1]
		if index <= n then
			s = t[2]
			break
		end
	end

	s = string.gsub(s, "%%NICK%%", nickinfo.nick)
	s = string.gsub(s, "%%CHAN%%", nickinfo.chan)
	s = string.gsub(s, "%%NET%%", nickinfo.net)
	s = string.gsub(s, "%%TOT%%", nickinfo.tot)
	s = string.gsub(s, "%%SUCC%%", nickinfo.succ)
	s = string.gsub(s, "%%CUTT%%", nickinfo.cutt)
	s = string.gsub(s, "%%TIME%%", nickinfo.time)
	return s
end

function show_dicks()
	local chan = irc.params(1)
	local echan = "'"..db:escape(chan).."'"
	local net = irc.network()
	local enet = "'"..db:escape(net).."'"
	local nick = string.match(irc.tail(), "%S+%s+(%S+)")
	if nick == nil then
		nick = privmsg.get_nick(irc)
	end

	nick = ualias.main_alias(net, nick)

	local enick = "'"..db:escape(nick).."'"

	local inchan = " WHERE channel = "..echan.." AND network = "..enet
	local fornick = inchan.." AND lower(nick) = lower("..enick..")"

	local res = {}
	nickinfo = {}
	local cur = db:execute("SELECT nick, goodact, badact FROM irc_hunters"..fornick)
	if cur ~= nil then
		cur:fetch(res)
	end
	if res[1] == nil then
		privmsg.reply(irc, "It seems "..nick.." has never participated to the dickhunt in "..chan..".")
		return
	end

	nickinfo.nick = res[1]
	nickinfo.succ = res[2]
	nickinfo.cutt = res[3]

	cur = db:execute("SELECT avg(t) FROM irc_hunter_times"..fornick)
	if cur ~= nil then
		cur:fetch(res)
	end
	if res[1] ~= nil then
		nickinfo.avg = res[1]
		res[1] = nil
	end

	-- I'm so sorry
	cur = db:execute("SELECT t FROM irc_hunter_times"..fornick.." ORDER BY t ASC LIMIT 1 OFFSET (SELECT CEIL(COUNT(t) / 2) FROM irc_hunter_times"..fornick..")")
	if cur ~= nil then
		cur:fetch(res)
	end
	if res[1] ~= nil then
		nickinfo.med = res[1]
	end

	local s = nickinfo.nick.." has sucked "..nickinfo.succ.." and cut "..nickinfo.cutt.." dicks in "..chan.." with"
	if nickinfo.avg ~= nil then
		s = s.." an average time of "..nickinfo.avg.." seconds"
	end

	if nickinfo.med ~= nil then
		if nickinfo.avg ~= nil then
			s = s.." and"
		end

		s = s.." a median of "..nickinfo.med.." seconds"
	end

	s = s.."."

	privmsg.reply(irc, s)
end

function show_suckers()
	show_best("succ")
end

function show_cutters()
	show_best("cutt")
end

function show_best(act)
	local dbact, adj
	if act == "succ" then
		dbact = "goodact"
		adj = "suckers"
	else
		dbact = "badact"
		adj = "cutters"
	end

	local chan = irc.params(1)
	local echan = "'"..db:escape(chan).."'"
	local net = irc.network()
	local enet = "'"..db:escape(net).."'"
	local inchan = " WHERE channel = "..echan.." AND network = "..enet

	nickinfo = {}
	cur = db:execute("SELECT nick, "..dbact.." FROM irc_hunters"..inchan.." AND "..dbact.." >= 5 ORDER BY "..dbact.." DESC LIMIT 10")

	if cur == nil then
		privmsg.reply(irc, "Nobody seems to have participated in the hunt in "..chan..".")
		return
	end

	local s = "Best "..adj.." in \x02"..chan.."\x02:"
	while cur:fetch(nickinfo) ~= nil do
		s = s.."\x02"..nickinfo[1].."\x02 : "..nickinfo[2].." - "
	end

	s = string.gsub(s, "%s%-%s$", ".")
	privmsg.reply(irc, s)
end

function send_decoy()
	local chan = irc.params(1)

	if privmsg.sent_by_mod(irc) then
		privmsg.reply(irc, dickgen.gen())
	end
end

-- The entire sender table should not use the privmsg library
-- or the message parsing functions from the IRC table, since
-- it's called by periodics
sender = {}

sender.max_init_score = 25

function sender.init_score()
	math.randomseed(osr.random())
	return math.random(0, sender.max_init_score)
end

function sender.send(chan, net)
	db:execute("UPDATE irc_hunts SET sent = 't', lastact = now() WHERE channel = '"..db:escape(chan).."' AND network = '"..db:escape(net).."'")
	irc.msg(chan, dickgen.gen(), net)
end

function sender.sched()
	local hunt = {}
	local cur = db:execute("SELECT EXTRACT(EPOCH FROM age(now(), lastact)), score, channel, network FROM irc_hunts WHERE state = 't' AND sent = 'f'")

	if cur == nil then
		return
	end

	while cur:fetch(hunt) ~= nil do
		local t = tonumber(hunt[1])
		local x = tonumber(hunt[2])

		if sender.is_ready(t, x) == true then
			sender.send(hunt[3], hunt[4])
		end
	end
end

function sender.is_ready(t, x)
	-- no dick should be sent before *min
	-- a dick should absolutely be sent after *max,
	-- unless one of the params are below *min
	local xmin = 25
	local xmax = 150
	local tmin = 1200 -- min 20 minutes
	local tmax = 10800 -- max 3 hours

	if x < xmin or t < tmin then
		return false
	else
		return t > sender.func(x, xmin, xmax, tmin, tmax)
	end
end

function sender.func(x, xmin, xmax, tmin, tmax)
	local k = xmax / (math.sqrt(xmax - xmin))
	return ((-k * math.sqrt(x - xmin)) + tmax)
end

sched = sender.sched;
irc.reghook("PRIVMSG", "start", "starthunt")
irc.reghook("PRIVMSG", "stop", "stophunt")
irc.reghook("PRIVMSG", "show_suckers", "suckers", "succers")
irc.reghook("PRIVMSG", "show_cutters", "cutters")
irc.reghook("PRIVMSG", "show_dicks", "dicks")
irc.reghook("PRIVMSG", "send_decoy", "decoy")
irc.reghook("PRIVMSG", "handle_msg")
irc.periodic("sched", 5)
