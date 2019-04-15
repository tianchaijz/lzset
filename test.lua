local cjson = require "cjson.safe"
local zset = require "zset"
local zset_int = require "lzset.int"
local zset_string = require "lzset.string"


local function equal(a, b)
    if type(a) ~= type(b) then return false end
    if type(a) == "table" then return cjson.encode(a) == cjson.encode(b) end
    return a == b
end


local zs = zset_string()
assert(#zs == 0)

zs:insert(10, "a")
zs:insert(11, "b")
zs:insert(12, "c")

assert(#zs == 3)
assert(zs:get_rank(12, "b") == 2)
assert(zs:get_score_rank(3) == 0)
assert(zs:get_score_rank(12, true) == 2)
assert(zs:get_score_rank(12, false) == 3)
assert(zs:get_score_rank(100) == 3)
assert(equal(zs:get_range_by_rank(1, 1), { "a" }))
assert(equal(zs:get_range_by_rank(1, 2), { "a", "b" }))
assert(equal(zs:get_range_by_rank(1, 3), { "a", "b", "c" }))
assert(equal(zs:get_range_by_rank(1, 4), { "a", "b", "c" }))


zs = zset_string()


local total = 500000
for i = 1, total do
    zs:insert(i, tostring(i))
    zs:insert(i, tostring(i))
end
assert(zs:count(), total)
assert(zs:get_rank(total, tostring(total)) == total)
for i = 1, total do
    local n = math.random(total)
    assert(zs:get_rank(n, tostring(n)) == n)
end

local a1, a2 = 100, 100000
local t1 = zs:get_range_by_rank(a1, a2)
local t2 = zs:get_range_by_rank(a2, a1)
assert(#t1 == #t2)
assert(#t1, a2 - a1 + 1)
for i, key in ipairs(t1) do
    assert(key == t2[#t2 -i + 1], key)
end


local function test_range(zs, r1, r2, func)
    local t = zs[func](zs, r1, r2)
    for i, key in ipairs(t) do
        if r1 < r2 then
            assert(t[i] == tostring(r1 + (i - 1)))
        else
            assert(t[i] == tostring(r1 - (i - 1)))
        end
    end
end


for i = 1, 10 do
    local r1 = math.random(total)
    local r2 = math.random(total)
    test_range(zs, r1, r2, "get_range_by_rank")
    test_range(zs, r1, r2, "get_range_by_score")
end


for i = 1, total do
    zs:delete(i, tostring(i))
end
assert(zs:count(), 0)


local function gen_zset(n)
    local zs = zset.new(zset.TYPE_INT)
    for i = 1, n do
        zs:insert(i, i)
    end
    return zs
end


zs = gen_zset(10)
assert(equal(zs:get_range_by_rank(2, 5), { 2, 3, 4, 5 }))
assert(equal(zs:get_range_by_score(2, 5), { 2, 3, 4, 5 }))


zs = gen_zset(3)
assert(equal(zs:get_range_by_rank(1, 3), { 1, 2, 3 }))
assert(equal(zs:get_range_by_score(1, 3), { 1, 2, 3 }))
assert(equal(zs:get_range_by_score(1, 1), { 1 }))
assert(equal(zs:get_range_by_score(2, 2), { 2 }))


print("test insert")
zs = gen_zset(10)
zs:insert(11, 11)
assert(zs:count() == 11)
assert(zs:score(11) == 11)
zs:insert(5, 11)
assert(zs:score(11) == 5)
assert(zs:rank(11) == 6)


print("test delete")
zs = gen_zset(10)
zs:delete(10)
assert(zs:count() == 9)
assert(zs:score(10) == nil)


print("test remove less")
zs = gen_zset(10)
zs:remove_lt(0)
assert(zs:count() == 10)

zs:remove_lt(1)
assert(zs:count() == 10)

zs:remove_lte(1)
assert(zs:count() == 9)

zs:remove_lt(10)
assert(zs:count() == 1)

zs = gen_zset(10)
zs:remove_lte(10)
assert(zs:count() == 0)

zs = gen_zset(10)
zs:remove_lt(5)
assert(zs:count() == 6)

zs = gen_zset(10)
zs:remove_lte(5)
assert(zs:count() == 5)


print("test remove greater")
zs = gen_zset(10)
zs:remove_gt(10)
assert(zs:count() == 10)

zs = gen_zset(10)
zs:remove_gte(10)
assert(zs:count() == 9)

zs = gen_zset(10)
zs:remove_gt(1)
assert(zs:count() == 1)

zs = gen_zset(10)
zs:remove_gte(1)
assert(zs:count() == 0)

zs = gen_zset(10)
zs:remove_gt(0)
assert(zs:count() == 0)

zs = gen_zset(10)
zs:remove_gte(0)
assert(zs:count() == 0)

zs = gen_zset(10)
zs:remove_gt(5)
assert(zs:count() == 5)

zs = gen_zset(10)
zs:remove_gte(5)
assert(zs:count() == 4)


print("test limit")
zs = gen_zset(10)
zs:limit_front(0)
assert(zs:count() == 0)

zs = gen_zset(10)
zs:limit_back(0)
assert(zs:count() == 0)

zs = gen_zset(10)
zs:limit_front(5)
assert(zs:count() == 5)
assert(zs:rank(5) == 5)

zs = gen_zset(10)
zs:limit_back(5)
assert(zs:count() == 5)
assert(zs:rank(6) == 1)

zs = gen_zset(10)
zs:limit_back(1)
assert(zs:count() == 1)
assert(zs:rank(10) == 1)


print("test delete cb")
zs = gen_zset(10)
zs:limit_front(0, function(key) end)
assert(zs:count() == 0)


collectgarbage("collect")
print("gc before: ", collectgarbage("count"))
zs = nil
collectgarbage("collect")
print("gc after: ", collectgarbage("count"))


print("OK!")
