local lzset_int = require "lzset.int"
local lzset_string = require "lzset.string"


local setmetatable = setmetatable


local _M = { TYPE_INT = 0, TYPE_STRING = 1 }
local _mt = { __index = _M }


function _M.new(typ)
    local zset = {
        _sl = typ == _M.TYPE_INT and lzset_int() or lzset_string(),
        _dict = {},
    }

    setmetatable(zset, _mt)

    return zset
end


function _M.insert(self, score, key)
    local curscore = self._dict[key]
    if curscore then
        if curscore ~= score then
            self._sl:update(curscore, key, score)
            self._dict[key] = score
        end
        return
    end

    self._sl:insert(score, key)
    self._dict[key] = score
end


function _M.delete(self, key)
    local score = self._dict[key]
    if score then
        self._sl:delete(score, key)
        self._dict[key] = nil
    end
end


function _M.count(self)
    return #self._sl
end


-- remove [from, to]
function _M._remove_helper(self, from, to, cb)
    local delete_cb = function(key)
        self._dict[key] = nil
        if cb then cb(key) end
    end

    return self._sl:delete_range_by_rank(from, to, delete_cb)
end


-- remove (-∞, score)
function _M.remove_lt(self, score, cb)
    local rank = self._sl:get_score_rank(score, true)
    if rank == 0 then
        return
    end

    return self:_remove_helper(1, rank, cb)
end


-- remove (-∞, score]
function _M.remove_lte(self, score, cb)
    local rank = self._sl:get_score_rank(score, false)
    if rank == 0 then
        return
    end

    return self:_remove_helper(1, rank, cb)
end


-- remove (score, +∞)
function _M.remove_gt(self, score, cb)
    local rank = self._sl:get_score_rank(score, false) + 1
    local count = #self._sl
    if rank > count then
        return
    end

    return self:_remove_helper(rank, #self._sl, cb)
end


-- remove [score, +∞)
function _M.remove_gte(self, score, cb)
    local rank = self._sl:get_score_rank(score, true) + 1
    local count = #self._sl
    if rank > count then
        return
    end

    return self:_remove_helper(rank, #self._sl, cb)
end


function _M.limit_front(self, n, cb)
    local count = #self._sl
    if n >= count then
        return 0
    end

    return self:_remove_helper(n + 1, count, cb)
end


function _M.limit_back(self, n, cb)
    local count = #self._sl
    if n >= count then
        return 0
    end

    local to = count - n

    return self:_remove_helper(1, to, cb)
end


function _M.get_range_by_rank(self, r1, r2)
    if r1 < 1 then r1 = 1 end
    if r2 < 1 then r2 = 1 end

    return self._sl:get_range_by_rank(r1, r2)
end


function _M.get_range_by_score(self, s1, s2)
    return self._sl:get_range_by_score(s1, s2)
end


function _M.rank(self, key)
    local score = self._dict[key]
    if not score then
        return nil
    end

    return self._sl:get_rank(score, key)
end


function _M.score(self, key)
    return self._dict[key]
end


function _M.reverse_rank(self, rank)
    return #self._sl - rank + 1
end


function _M.dump(self)
    self._sl:dump()
end


return _M
