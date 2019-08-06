privmsg = require("privmsg")
n = 0

function echo()
	local text = string.match(irc.tail(), "^%S+%s+(.+)$");
	if text == nil then
		return
	end

	n = n + 1
	privmsg.reply(irc, text .. ": " .. n)
end

irc.reghook("PRIVMSG", "echo", "echo", "echo2")
