Packer = {}
Packer.Rect = {}
Packer.Node = {}

function Packer.new()
    local obj = {}
    setmetatable(obj, { __index = Packer })
    return obj
end

-- class Packer.Rect
function Packer.Rect.new(x, y, w, h)
    local obj = {}
    setmetatable(obj, { __index = Packer.Rect })
    obj:setRect(x or 0, y or 0, w or 0, h or 0)
    obj.object = nil
    return obj
end

function Packer.Rect:setRect(x, y, w, h)
    self.x = x
    self.y = y
    self.w = w
    self.h = h
end

function Packer.Rect:alignSize(aw, ah)
    if aw > 0 then
        self.w = math.tointeger((self.w + aw - 1) / aw) * aw
    end
    if ah > 0 then
        self.h = math.tointeger((self.h + ah - 1) / ah) * ah
    end
end

-- class Packer.Node
function Packer.Node.new(rc)
    local obj = {}
    setmetatable(obj, { __index = Packer.Node })
    obj.left = nil
    obj.right = nil
    obj.rect = rc
    return obj
end

function Packer.Node:insert(rc, margin)
    if self.rect == nil then
        if self.left:insert(rc, margin) then
            return true
        end
        return self.right:insert(rc, margin)
    end
    if (rc.w + margin > self.rect.w) or (rc.h + margin > self.rect.h) then
        return false
    end
    rc.x = self.rect.x
    rc.y = self.rect.y
    local w = rc.w + margin
    local h = rc.h + margin
    local dw = self.rect.w - w
    local dh = self.rect.h - h
    if dw > dh then
        self.left = Packer.Node.new(Packer.Rect.new(self.rect.x, self.rect.y + h, w, dh))
        self.right = Packer.Node.new(Packer.Rect.new(self.rect.x + w, self.rect.y, dw, self.rect.h))
    else
        self.left = Packer.Node.new(Packer.Rect.new(self.rect.x + w, self.rect.y, dw, h))
        self.right = Packer.Node.new(Packer.Rect.new(self.rect.x, self.rect.y + h, self.rect.w, dh))
    end
    self.rect = nil
    return true
end

function Packer:pack(image_rects, margin)
    table.sort(image_rects, function(a, b) return math.max(b.w, b.h) < math.max(a.w, a.h) end)
    local w, h = self.calcInitialRect(image_rects)
    while true do
        local root = Packer.Node.new(Packer.Rect.new(0, 0, w, h))
        local result = self:insertImagesToRoot(image_rects, root, margin)
        if result then
            return w, h
        end
        if w > h then
            h = h + 8
        else
            w = w + 8
        end
    end
end

function Packer:insertImagesToRoot(image_rects, root, margin)
    for _, v in ipairs(image_rects) do
        if root:insert(v, margin) == false then
            return false
        end
    end
    return true
end

function Packer.calcInitialRect(image_rects)
    local total = Packer.calcTotalPixel(image_rects)
    local len = math.floor(math.sqrt(total)) / 2
    local w = Packer.align(len, 8)
    local h = len

    while w * h < total do
        if w > h then
            h = h + 8
        else
            w = w + 8
        end
    end
    return w, h
end

function Packer.calcTotalPixel(image_rects)
    local total = 0
    for _, v in ipairs(image_rects) do
        total = total + v.w * v.h
    end
    return total
end

function Packer.align(x, a)
    return math.tointeger((x + a - 1) / a) * a
end
