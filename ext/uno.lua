privmsg = require("privmsg")
rex = require("rex_pcre2")
osr = require("osrandom")

dbdriver = require("luasql.postgres")
dbenv = dbdriver.postgres()
db = dbenv:connect("ircbot", "ircbot", "canttouchthis", "192.168.90.255")

whole_deck = {
--	id = {
--		name = '',
--		type = '',
--		colour = '',
--		special = false,
--		value = 0,
--		id = id
--	}
}

games = {
--	network = {
--		channel = {
--			current_player = (player)
--			players = {
--				id = {
--					id = (id),
--					name = (name),
--					hand = {(cards)},
--					next = (player),
--					prev = (player)
--				}
--			},
--			deck = {(cards)},
--			pile = {(cards)},
--			allowed = false,
--			starting = false,
--			playing = false,
--			challenging = false,
--			challenge_from = nil,
--			challenge_to = nil,
--			drawn = (card),
--			order = true -- true goes to next, false goes to previous
--		},
--	}
}

db:execute("CREATE TABLE IF NOT EXISTS uno_stats (network varchar(32), channel varchar(256), player varchar(64), score int, wins int, games int, UNIQUE(network, channel, player))")
db:setautocommit(true)

function dbstr(str)
	if str == nil then
		print("String sent to dbstr is nil")
		print(debug.traceback())
	else
		return "'"..db:escape(str).."'"
	end
end

function trand(t)
	math.randomseed(osr.random())
	local size = 0
	for k, v in pairs(t) do
		size = size + 1
	end

	local index
	if size > 1 then
		index = math.random(1, size)
	else
		return t[1]
	end

	for k, v in pairs(t) do
		index = index - 1
		if index == 0 then
			return v
		end
	end
end

function add_card(amount, name, type, colour, special, value)
	local count = #whole_deck
	for i = 1, amount, 1 do
		whole_deck[count + 1] = {
			name = name,
			type = type,
			colour = colour,
			special = special,
			value = value,
			id = count + 1
		}
	end
end

function populate_deck()
	for _, col in pairs({"red", "yellow", "green", "blue"}) do
		add_card(1, col.." 0", "0", col, false, 0)
		add_card(2, col.." draw-two", "d2", col, true, 20)
		add_card(2, col.." reverse", "r", col, true, 20)
		add_card(2, col.." skip", "s", col, true, 20)
		for i = 1, 9, 1 do
			add_card(2, col.." "..i, tostring(i), col, false, i)
		end
	end

	add_card(4, "wild card", "w", "wild", false, 50)
	add_card(4, "wild draw-four", "wd4", "wild", true, 50)
end

populate_deck()

function change_uno_status(status)
	local chan = irc.params(1)
	local net = irc.network()

	if not privmsg.sent_by_mod(irc) then
		privmsg.reply("Only a moderator can enable or disable uno")
	end

	games[net][chan].allowed = status

	if status then
		privmsg.reply(irc, "Enabled uno in "..chan)
	else
		privmsg.reply(irc, "Disabled uno in "..chan)
	end
end

function show_stats()
	local text = irc.tail()
	local nick = string.match(text, "^%S+%s+%S+%s+(%S+)")
	local chan = irc.params(1)
	if nick == nil then
		nick = privmsg.get_nick(irc)
	end

	local enick = dbstr(nick)
	local enet = dbstr(irc.network())
	local echan = dbstr(chan)
	local cur = db:execute("SELECT * FROM uno_stats WHERE network = "..enet.." AND channel = "..echan.." AND player = "..enick)
	local stats = {}
	if cur == nil or cur:fetch(stats, "a") == nil then
		privmsg.reply(irc, "No stats found for player "..nick.." in "..chan)
		return
	end

	local winrate = string.format("%.2f", (tonumber(stats.wins) / tonumber(stats.games)) * 100)
	local ppg = string.format("%.2f", tonumber(stats.score) / tonumber(stats.games))
	privmsg.reply(irc, "Stats for "..nick..": "..stats.score.." points from "..stats.wins.." wins out of "..stats.games.." games ("..winrate.."% winrate) with "..ppg.." points per game.")
end

function show_scores()
	local echan = dbstr(irc.params(1))
	local enet = dbstr(irc.network())
	local cur = db:execute("SELECT player, wins, games, score FROM uno_stats WHERE network = "..enet.." AND channel = "..echan.." AND score >= 0 ORDER BY score DESC LIMIT 10")
	local text = "Top players:"
	local stat = {}

	while cur:fetch(stat, "a") ~= nil do
		text = text.." "..stat.player.."["..stat.wins.."/"..stat.games.."]: "..stat.score.." points -"
	end

	text = string.gsub(text, "%s%-$", ".")
	privmsg.reply(irc, text)
end

function show_help()
	privmsg.reply_notice(irc, "To learn to play uno: https://service.mattel.com/instruction_sheets/42001pr.pdf - The bot is 100% compliant to these rules.")
	privmsg.reply_notice(irc, "Use %uno start to setup the game, %uno join to join, %uno begin to deal the cards and start playing. %uno cards gives you a briefing on the cards.")
	privmsg.reply_notice(irc, "When the bot announces your turn, you can spend it by either drawing a card or playing a card. To have it notify you of your hand, type %uno hand. %draw or %d draws a card, %play or %p plays a card with the syntax: %p <colour> <card>, after drawing, %skip or %s skips your turn.")
end

function show_cards()
	privmsg.reply_notice(irc, "tl;dr: args are [colour number] for number cards, [colour r/s/d2] for reverse, skip and draw-two, [w/wd4 newcolour] for wild and wild draw-four. Newcolour is the colour they'll act as. To learn more about cards and their impact, see https://service.mattel.com/instruction_sheets/42001pr.pdf.")
end

function get_game_status()
	local net = irc.network()
	local chan = irc.params(1)
	local text

	if games[net][chan].starting then
		text = "We're setting up the game. Current players are: "..get_current_players(net, chan).."."
		privmsg.reply(irc, text)
		return
	elseif not games[net][chan].playing then
		privmsg.reply(irc, "There's no game going on.")
		return
	end

	local player = games[net][chan].current_player
	local top = get_pile_top(net, chan)
	text = "It's "..player.name.."'s turn | "..get_player_info(net, chan).." | top card is a "..clcard(top)
	if top.type == "wd4" or top.type == "w" then
		text = text.." ("..cltext(top.colour, top.colour)..")"
	end

	text = text.."."
	privmsg.reply(irc, text)
end

function get_player_info(net, chan)
	local text = ''
	local curp = games[net][chan].current_player
	local player = curp

	repeat
		text = text..player.name.."["..#player.hand.."]".." "

		if games[net][chan].order then
			player = player.next
		else
			player = player.prev
		end
	until player == curp

	text = string.gsub(text, "%s$", "")
	return text
end

function show_hand()
	local nick = privmsg.get_nick(irc)
	local net = irc.network()
	local chan = irc.params(1)

	if games[net][chan].playing and get_player(net, chan, nick) ~= nil then
		privmsg.reply_notice(irc, get_hand(get_player(net, chan, nick))..".")
	end
end

function uno_start()
	local net = irc.network()
	local chan = irc.params(1)
	local nick = privmsg.get_nick(irc)

	if games[net][chan].starting then
		privmsg.reply(irc, "A game is starting already! Type %uno join to play.")
		return
	elseif games[net][chan].playing then
		privmsg.reply(irc, "There's already a game! Wait until it's done.")
		return
	end

	add_player(net, chan, nick)
	games[net][chan].starting = true
	privmsg.reply(irc, "Starting a game in "..chan)
end

function uno_begin()
	local net = irc.network()
	local chan = irc.params(1)
	if not games[net][chan].starting then
		privmsg.reply(irc, "Please use %uno start first to setup the game.")
		return
	elseif games[net][chan].playing then
		privmsg.reply(irc, "The game has already begun.")
		return
	elseif get_player_count(net, chan) < 2 then
		privmsg.reply(irc, "You can't start the game unless there are two or more players in it.")
		return
	end

	init_deck(net, chan)
	deal_hands(net, chan)
	init_players(net, chan)
	privmsg.reply(irc, "Current players are: "..get_current_players(net, chan)..".")
	init_pile(net, chan)

	games[net][chan].starting = false
	games[net][chan].playing = true
	privmsg.reply(irc, "The game has started! Type %p <colour> <card> to play a card when it's your turn, %d to draw a card, %uno leave to leave the game. The first person to discard all of their cards wins. The game can be stopped with %uno stop.")

	begin_turn(net, chan)
end

function cleanup_game(net, chan)
	games[net][chan].deck = {}
	games[net][chan].pile = {}
	games[net][chan].players = {}
	games[net][chan].starting = false
	games[net][chan].playing = false
	games[net][chan].challenge_from = nil
	games[net][chan].challenge_to = nil
	games[net][chan].drawn = 0
	games[net][chan].order = true
end

function init_deck(net, chan)
	games[net][chan].deck = {}
	games[net][chan].pile = {}
	for id, card in pairs(whole_deck) do
		games[net][chan].deck[id] = card
	end
end

function deal_hands(net, chan)
	for k, player in pairs(games[net][chan].players) do
		draw_cards(7, net, chan, player)
	end
end

function init_players(net, chan)
	local player = trand(games[net][chan].players)
	games[net][chan].current_player = player
end

function get_current_players(net, chan)
	local text = ""
	for k, player in pairs(games[net][chan].players) do
		text = text..player.name..", "
	end

	text = string.gsub(text, ",%s$", "")
	return text
end

function init_pile(net, chan)
	local card = {}
	repeat card = trand(games[net][chan].deck) until card.type ~= 'wd4'
	games[net][chan].pile[#games[net][chan].pile + 1] = card
	games[net][chan].deck[card.id] = nil

	privmsg.reply(irc, "Top card is a "..clcard(card)..".")
	apply_specials(net, chan, card, games[net][chan].current_player)
end

function add_player(net, chan, nick)
	local index = #games[net][chan].players + 1
	local players = games[net][chan].players
	players[index] = {
		id = index,
		name = nick,
		hand = {}
	}

	if index == 1 then
		players[1].next = players[1]
		players[1].prev = players[1]
	else
		players[index].next = players[1]
		players[index].prev = players[1].prev
		players[1].prev.next = players[index]
		players[1].prev = players[index]
	end
end

function uno_join()
	local net = irc.network()
	local chan = irc.params(1)
	local nick = privmsg.get_nick(irc)

	if games[net][chan].playing then
		privmsg.reply(irc, "There's already a game going on.")
		return
	end

	if games[net][chan].starting then
		if get_player(net, chan, nick) == nil then
			add_player(net, chan, nick)
			privmsg.reply(irc, nick.." is joining the game.")
		else
			privmsg.reply(irc, "You're already joining the game")
		end
	else
		privmsg.reply(irc, "No game is starting, you can start one with %uno start.")
	end
end

function uno_leave()
	local net = irc.network()
	local chan = irc.params(1)
	local nick = privmsg.get_nick(irc)
	local player = get_player(net, chan, nick)

	if not games[net][chan].playing then
		privmsg.reply(irc, "There's no game to leave.")
		return
	elseif player == nil then
		privmsg.reply(irc, "You can't leave a game you're not a part of.")
		return
	end

	player.prev.next = player.next
	player.next.prev = player.prev
	table.remove(games[net][chan].players, player.id)

	if get_player_count(net, chan) <= 1 then
		uno_stop()
	else
		privmsg.reply(irc, nick.." has left the game.")
		if not games[net][chan].starting then
			uno_status()
		end
	end
end

function uno_stop()
	local net = irc.network()
	local chan = irc.params(1)
	local nick = privmsg.get_nick(irc)

	if not games[net][chan].playing and not games[net][chan].starting then
		privmsg.reply(irc, "There's no game to stop.")
		return
	elseif not privmsg.sent_by_mod(irc) and get_player(net, chan, nick) == nil then
		privmsg.reply(irc, "You can't stop a game you're not a part of.")
		return
	end

	cleanup_game(net, chan)
	privmsg.reply(irc, "The game in "..chan.." was stopped.")
end

function wd4_challenge_act(agreed)
	local net = irc.network()
	local chan = irc.params(1)
	local nick = privmsg.get_nick(irc)

	if games[net][chan].playing == false then
		privmsg.reply(irc, "There's no game going on.")
		return
	elseif get_player(net, chan, nick) == nil then
		privmsg.reply(irc, "You're not in the game!")
		return
	elseif not games[net][chan].challenging then
		privmsg.reply(irc, "There's nobody being challenged.")
		return
	elseif games[net][chan].current_player.name ~= nick and games[net][chan].challenge_to == nick then
		privmsg.reply(irc, "You're not the one being challenged.")
		return
	end

	if agreed then
		if is_wd4_legal(net, chan) then
			wd4_challenge_succeeded(net, chan)
		else
			wd4_challenge_failed(net, chan)
		end
	else
		wd4_challenge_refused(net, chan)
	end

	games[net][chan].challenging = false
	games[net][chan].challenge_from = nil
	games[net][chan].challenge_to = nil
end

function end_turn(net, chan)
	games[net][chan].drawn = 0

	local player = games[net][chan].current_player
	local nextplayer = get_next_player(net, chan)
	local card = get_pile_top(net, chan)

	if is_winner(net, chan, player) then
		victor(net, chan, player)
	else
		apply_specials(net, chan, card, nextplayer)
		next_turn(net, chan)
		begin_turn(net, chan)
	end
end

function begin_turn(net, chan)
	local player = games[net][chan].current_player
	irc.notice(player.name, "You have: "..get_hand(player)..".")
	get_game_status()
end

function is_winner(net, chan, player)
	return (#player.hand == 0)
end

function victor(net, chan, player)
	init_stats(net, chan)
	log_game(net, chan, player)
	uno_stop()
end

function init_stats(net, chan)
	local enet = dbstr(net)
	local echan = dbstr(chan)

	for k, player in pairs(games[net][chan].players) do
		db:execute("INSERT INTO uno_stats (network, channel, player, score, wins, games) VALUES ("..enet..", "..echan..", "..dbstr(player.name)..", 0, 0, 0) ON CONFLICT DO NOTHING")
	end
end

function get_winner_score(net, chan)
	local score = 0
	for i, player in pairs(games[net][chan].players) do
		for j, card in pairs(player.hand) do
			score = score + card.value
		end
	end

	return score
end

function log_game(net, chan, winner)
	local ischan = "network = "..dbstr(net).." AND channel = "..dbstr(chan)
	local score = get_winner_score(net, chan)
	privmsg.reply(irc, winner.name.." won with "..score.." points!")
	db:execute("UPDATE uno_stats SET score = score + "..score..", wins = wins + 1 WHERE "..ischan.." AND player = "..dbstr(winner.name))
	db:execute("UPDATE uno_stats SET games= games + 1 WHERE "..ischan.." AND player IN (SELECT player FROM uno_players WHERE "..ischan..")")
end

function next_turn(net, chan)
	local nextplayer = get_next_player(net, chan)
	games[net][chan].current_player = nextplayer
end

function skip_turn(net, chan, player)
	next_turn(net, chan)
	privmsg.reply(irc, player.name.."'s turn was skipped.")
end

function draw_two(net, chan, player)
	next_turn(net, chan)
	draw_cards(2, net, chan, player)
	privmsg.reply(irc, player.name.." has drawn 2 cards, and their turn will be skipped.")
end

function wild_draw_four(net, chan, player)
	local from = games[net][chan].current_player
	games[net][chan].challenging = true
	games[net][chan].challenge_from = from
	games[net][chan].challenge_to = player
	privmsg.reply(irc, player.name.." would you like to challenge the wd4 played by "..from.name.."? (;uno yes/no)")
end

function reverse_order(net, chan, player)
	if get_player_count(net, chan) > 2 then
		games[net][chan].order = not games[net][chan].order
	else
		next_turn(net, chan)
	end
end

specials = {
	s = skip_turn,
	d2 = draw_two,
	wd4 = wild_draw_four,
	r = reverse_order
}

function apply_specials(net, chan, card, player)
	if card.special and specials[card.type] ~= nil then
		specials[card.type](net, chan, player)
	end

	local pile = games[net][chan].pile
	pile[#pile].special = false
end

function wd4_challenge_succeeded(net, chan)
	local challenger = games[net][chan].challenge_from
	draw_cards(4, net, chan, challenger)
	local hand = get_hand(challenger)
	privmsg.reply(irc, "The challenge succeeded! "..challenger.name.." has drawn 4 cards and their hand was shown to "..privmsg.get_nick(irc)..".")
	privmsg.reply_notice(irc, challenger.name.." has "..hand)
end

function wd4_challenge_failed(net, chan)
	local challenger = games[net][chan].challenge_from

	local nick = privmsg.get_nick(irc)
	local hand = get_hand(get_player(net, chan, nick))
	draw_cards(6, net, chan, get_player(net, chan, nick))
	irc.notice(challenger.name, nick.." has "..hand)
	privmsg.reply(irc, "The challenge failed, "..challenger.name.." has seen "..nick.."'s hand, "..nick.." has drawn 6 cards and their turn will be skipped.")
	end_turn(net, chan)
end

function wd4_challenge_refused(net, chan)
	local nick = privmsg.get_nick(irc)
	draw_cards(4, net, chan, get_player(net, chan, nick))
	privmsg.reply(irc, "The wd4 was not challenged, "..nick.." has drawn 4 cards and their turn will be skipped.")
	end_turn(net, chan)
end

function is_wd4_legal(net, chan)
	local top = games[net][chan].pile[#games[net][chan].pile - 1]
	for k, card in pairs(games[net][chan].challenge_to.hand) do
		if card.colour == top.colour then
			return false
		end
	end

	return true
end

-- stateless commands
local stl_commands = {
	on = function() change_uno_status(true) end,
	off = function() change_uno_status(false) end,
	scoreboard = show_scores,
	help = show_help,
	stats = show_stats,
	cards = show_cards
}

-- stateful commands
local stf_commands = {
	status = get_game_status,
	hand = show_hand,
	start = uno_start,
	begin = uno_begin,
	deal = uno_begin,
	join = uno_join,
	leave = uno_leave,
	stop = uno_stop,
	yes = function() wd4_challenge_act(true) end,
	no = function() wd4_challenge_act(false) end
}

function uno()
	local command = string.match(irc.tail(), "^%S+%s+(%S+)")
	local net = irc.network()
	local chan = irc.params(1)

	if command == nil then
		show_help()
		return
	end

	if games[net] == nil then
		games[net] = {}
	end

	if games[net][chan] == nil then
		games[net][chan] = {
			players = {},
			deck = {},
			pile = {},
			allowed = true,
			starting = false,
			playing = false,
			challenging = false,
			order = true
		}
	end

	if stl_commands[command] ~= nil then
		stl_commands[command]()
		return
	end

	if games[net][chan].allowed then
		if stf_commands[command] ~= nil then
			stf_commands[command]()
			return
		end
	else
		privmsg.reply(irc, "This channel has uno disabled. A moderator can use %uno on to enable it.")
		return
	end

	privmsg.reply(irc, "Unknown command: \""..command.."\".")
	return
end

function play_allowed(net, chan, nick)
	if games[net] == nil or games[net][chan] == nil or not games[net][chan].playing then
		privmsg.reply(irc, "Nobody's playing uno...")
		return false
	end

	if games[net][chan].starting then
		privmsg.reply(irc, "Relax, we're starting the game! Geeze!")
		return false
	end

	if games[net][chan].challenging then
		privmsg.reply(irc, "The game is currently on hold for a wd4 challenge")
		return false
	end

	if get_player(net, chan, nick) == nil then
		privmsg.reply(irc, "You're not in the game!")
		return false
	end

	if games[net][chan].current_player.name ~= nick then
		privmsg.reply(irc, "It's not your turn yet!")
		return false
	end

	return true
end

function get_player(net, chan, nick)
	for k, player in pairs(games[net][chan].players) do
		if nick == player.name then
			return player
		end
	end

	return nil
end

function get_next_player(net, chan)
	if games[net][chan].order then
		return games[net][chan].current_player.next
	else
		return games[net][chan].current_player.prev
	end
end

function get_player_count(net, chan)
	local n = 0
	for k, player in pairs(games[net][chan].players) do
		n = n + 1
	end

	return n
end

function get_colour(text)
	local colours = { r = "red", g = "green", b = "blue", y = "yellow" }
	local text1, colour, text2 = rex.match(text, "^(.*?)((?:\\b[rgby])|(?:[rgby]\\b)|red|green|blue|yellow)(.*)$")
	return colours[colour], text1..text2
end

function get_type(text)
	if text == nil then
		return nil
	end

	local typenames = { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "r", "s", "d2", "w", "wd4" }
	local types = { rex.match(text, "(0|zero|null)|(1|one)|(2|two)|(3|three)|(4|four)|(5|five)|(6|six)|(7|seven)|(8|eight)|(9|nine)|(re?v?e?r?s?e?)|(sk?i?p?)|(d.*?(?:2|tw?o?))|(wi?l?d?[^(?:.*?d.*?(?:4|four)])|(wi?l?d?.*?d.*?(?:4|four))") }

	for i, type in pairs(types) do
		if type and type ~= nil then
			return typenames[i]
		end
	end

	return nil
end

function parse_played_card()
	local text = string.match(irc.tail(), "^%S+%s+(.*)$")
	if text == nil then
		return nil
	end

	local colour, newtext = get_colour(text)
	local type = get_type(newtext)

	if colour == nil or type == nil then
		return nil
	end

	if type == "w" or type == "wd4" then
		return "wild", type, colour
	end

	return colour, type
end

function cltext(col, text)
	local cols = {
		red = "04",
		blue = "12",
		green = "03",
		yellow = "08"
	}

	local colour = cols[col]
	if colour == nil then
		return "\x02"..text.."\x02"
	end

	return "\x02\x03"..colour..text.."\x03\x02"
end

function clcard(card)
	if card == nil or card.name == nil then
		print("clcard: received a bad card")
		print(debug.traceback())
	end

	return cltext(card.colour, card.name)
end

function get_hand(player)
	local text = ""
	for k, card in pairs(player.hand) do
		text = text..clcard(card)..", "
	end

	text = string.gsub(text, ",%s$", "")
	return text
end

function get_card_from_hand(net, chan, player, c)
	if c.id == nil and c.colour ~= nil and c.type ~= nil then
		for k, card in pairs(player.hand) do
			if c.colour == card.colour and c.type == card.type then
				return card, k
			end
		end
	elseif c.id ~= nil then
		for k, card in pairs(player.hand) do
			if c.id == card.id then	
				return card, k
			end
		end
	end

	return nil
end

function get_drawn(net, chan)
	local id = games[net][chan].drawn
	if id == 0 then
		return nil
	else
		return whole_deck[id]
	end
end

function verify_deck(net, chan)
	local n = 0
	for k, card in pairs(games[net][chan].deck) do
		n = n + 1
	end

	if n == 0 then
		rebuild_deck(net, chan)
	end
end

function rebuild_deck(net, chan)
	local card
	local pile = games[net][chan].pile
	local deck = games[net][chan].deck
	local pilesize = #pile

	for i = 0, pilesize - 1, 1 do
		card = pile[i]
		deck[card.id] = card
		pile[i] = nil
	end

	pile[1] = pile[pilesize]
	pile[pilesize] = nil
end

function cardsort_comp(c1, c2)
	if (c1.colour == c2.colour) then
		return c1.type < c2.type
	else
		return c1.colour < c2.colour
	end
end

function draw_cards(n, net, chan, player)
	local card
	local cards = {}
	local text = "You drew: "
	for i = 1, n, 1 do
		card = trand(games[net][chan].deck)
		cards[i] = card

		games[net][chan].deck[card.id] = nil
		table.insert(player.hand, card)

		text = text..clcard(card)
		if i == n then
			text = text.."."
		else
			text = text..", "
		end

		verify_deck(net, chan)
	end

	table.sort(player.hand, cardsort_comp)
	irc.notice(player.name, text)
	return cards
end

function get_pile_top(net, chan)
	return games[net][chan].pile[#games[net][chan].pile]
end

function play()
	local nick = privmsg.get_nick(irc)
	local chan = irc.params(1)
	local net = irc.network()
	local player = get_player(net, chan, nick)

	if not play_allowed(net, chan, nick) then
		return
	end

	local col, type, newcol = parse_played_card()
	if col == nil then
		return
	end

	local pcard = {}
	pcard.colour = col
	pcard.type = type

	local card, cindex = get_card_from_hand(net, chan, player, pcard)
	local drawn = get_drawn(net, chan)
	local top = get_pile_top(net, chan)

	if card == nil or card.name == nil then
		privmsg.reply(irc, "You don't have that card.")
		return
	end

	if drawn ~= nil and (drawn.col ~= card.col or drawn.type ~= card.type) then
		privmsg.reply(irc, "You can only play the card you've just drawn, or use %skip to skip your turn.")
		return
	end

	if not (top.colour == card.colour or top.type == card.type or card.colour == "wild" or top.colour == "wild") then	
		privmsg.reply(irc, "You can't play a "..clcard(card).." on a "..clcard(top)..".")
		return
	end

	if card.colour == "wild" then
		if newcol ~= nil then
			card.colour = newcol
		else
			privmsg.reply(irc, "Please specify a colour to set the wild card to.")
		end
	end

	table.remove(player.hand, cindex)
	table.insert(games[net][chan].pile, card)
	end_turn(net, chan)
end

function draw()
	local nick = privmsg.get_nick(irc)
	local chan = irc.params(1)
	local net = irc.network()

	if not play_allowed(net, chan, nick) then
		return
	end

	local drawn = get_drawn(net, chan)
	if drawn ~= nil then
		privmsg.reply(irc, "You've already drawn! Use %skip to skip your turn or play the card you've drawn.")
		return
	end

	local cards = draw_cards(1, net, chan, get_player(net, chan, nick))
	games[net][chan].drawn = cards[1].id
end

function skip()
	local nick = privmsg.get_nick(irc)
	local chan = irc.params(1)
	local net = irc.network()

	if not play_allowed(net, chan, nick) then
		return
	end

	local drawn = get_drawn(net, chan)
	if drawn == nil then
		privmsg.reply(irc, "You can only skip your turn after drawing a card.")
	else
		end_turn(net, chan)
	end
end

irc.reghook("PRIVMSG", "uno", "uno")
irc.reghook("PRIVMSG", "play", "p", "play")
irc.reghook("PRIVMSG", "draw", "d", "draw")
irc.reghook("PRIVMSG", "skip", "s", "skip", "pa", "pass")
