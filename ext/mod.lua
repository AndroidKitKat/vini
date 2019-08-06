privmsg = require("privmsg")

function retmode()
	local nick = privmsg.get_nick(irc)
	local channel = irc.params(1)

	if privmsg.cmd(irc, "mode?") then
		irc.msg(channel, irc.modes(channel, nick))
	end
end

function retmod()
	local nick = privmsg.get_nick(irc)
	local channel = irc.params(1)

	if privmsg.cmd(irc, "mod?") then
		if irc.ismod(channel, nick) then
			irc.msg(channel, "yes")
		else
			irc.msg(channel, "no")
		end
	end
end

irc.reghook("PRIVMSG", "retmode")
irc.reghook("PRIVMSG", "retmod")
