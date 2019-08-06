osr = require("osrandom")

function irand(t)
	return t[math.random(1, #t)]
end

dick = {
	balls = { '8', 'B', 'O', 'Q', 'C', 'E', "@" },
	shafts = { { '#', 10 }, { '~', 12 }, { '-', 15 }, { ':', 17 }, { '=', 8 } },
	tips = { 'D', 'o', "\xc2\xb7" },

	cumming = { "*spurt* *spurt*", "*SPLORT*", "*splash* *splash*" },
	big = { "*THONK*", "*SHLYUP*", "*SLAP*", "*SHLONK*", "*taps on your shoulder*" },
	small = { "OwO", "*blushes*", "hi...", "*hugs*" },
	weird = { "*flap* *flap*", "AAAAAAAAAAAAAAAAA", "*sssssssssss*" },
	generic = { "*slap* *slap*", "*rubs*" },

	seed = 0
}

function dick.gen()
	dick.seed = osr.random()
	math.randomseed(dick.seed)

	local balls = irand(dick.balls)
	local shafttype = irand(dick.shafts)
	local tip = irand(dick.tips)

	local shaftcomp = shafttype[1]
	local shaftlen = math.random(1, shafttype[2])

	local cumropes = math.random(0, 2)
	local cumming = (cumropes > 0 and shaftcomp ~= '~')

	local d = ""
	d = d..balls
	for i = 0, shaftlen, 1 do
		d = d..shaftcomp
	end

	d = d..tip

	local cumlen
	if cumming then
		if math.random(0, 1) == 1 then
			d = d..' '
		end

		for i = 1, cumropes, 1 do
			if math.ceil(shaftlen/2) <= 2 then
				cumlen = 1
			else
				cumlen = math.random(1, math.ceil(shaftlen/2))
			end
			for j = 1, cumlen, 1 do
				d = d..'~'
			end

			d = d..' '
		end
	else
		d = d..' '
	end

	if shaftlen < 4 then
		d = d..irand(dick.small)
	elseif shaftlen >= 8 and shaftcomp == '~' or shaftcomp == '-' then
		d = d..irand(dick.weird)
	elseif shaftlen >= 8 then
		d = d..irand(dick.big)
	elseif cumming then
		d = d..irand(dick.cumming)
	else
		d = d..irand(dick.generic)
	end

	return d
end

return dick
