privmsg = {}
prfx = { "%", ";", "@" }

function privmsg.nih(irc)
	local sender = irc.sender()
	return string.match(sender, "^(.+)!(.+)@(.+)$")
end

function privmsg.get_nick(irc)
	local sender = irc.sender()
	return string.match(sender, "^(.+)!.+@.+$")
end

function privmsg.get_host(irc)
	local sender = irc.sender()
	return string.match(sender, "^.+!.+@(.+)$")
end

function privmsg.sent_by_mod(irc)
	local nick = privmsg.get_nick(irc)
	local channel = irc.params(1)
	return irc.ismod(channel, nick)
end

function privmsg.cmd(irc, command)
	local text = irc.tail()
	prefix = string.sub(text, 1, 1)
	if string.len(text) > string.len(command) and
	   table_contains(prfx, prefix) and
	   string.sub(text, 2, string.len(command) + 1) == command then
		local args = string.sub(text, string.len(command) + 2)
		args = string.match(args, "^%s+(%S+)%s*$")
		return true, args
	end

	return false, nil
end

function privmsg.reply(irc, text)
	local nick = privmsg.get_nick(irc)
	local target = irc.params(1)
	if string.sub(target, 1, 1) == "#" then
		irc.msg(target, text)
	else
		irc.msg(nick, text)
	end
end

function privmsg.reply_action(irc, text)
	local nick = privmsg.get_nick(irc)
	local target = irc.params(1)
	if string.sub(target, 1, 1) == "#" then
		irc.action(target, text)
	else
		irc.action(nick, text)
	end
end

function privmsg.reply_notice(irc, text)
	local nick = privmsg.get_nick(irc)
	irc.notice(nick, text)
end

function table_contains(t, e)
	for k,v in pairs(t) do
		if v == e then
			return true
		end
	end

	return false
end

return privmsg
