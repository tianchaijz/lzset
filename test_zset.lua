local zset = require "zset"


local total = 100
local all = {}
for i = 1, total do
    all[#all + 1] = i
end


local function random_choose(t)
    if #t == 0 then
        return
    end
    local i = math.random(#t)
    return table.remove(t, i)
end


local zs = zset.new(zset.TYPE_STRING)

while true do
    local score = random_choose(all)
    if not score then
        break
    end
    local key = "a" .. score
    zs:insert(score, key)
end

assert(total == zs:count())


print("rank 28:", zs:rank("a28"))

local t = zs:get_range_by_rank(1, 10)
print("rank [1, 10]:")
for _, key in ipairs(t) do
    print(key)
end


local t = zs:get_range_by_rank(zs:reverse_rank(1), zs:reverse_rank(10))
print("reverse rank [1, 10]:")
for _, key in ipairs(t) do
    print(key)
end


print("------------------ dump ------------------")
zs:dump()

print("------------------ dump after limit front 10 ------------------")
zs:limit_front(10)
zs:dump()

print("------------------ dump after limit back 5 ------------------")
zs:limit_back(5)
zs:dump()
