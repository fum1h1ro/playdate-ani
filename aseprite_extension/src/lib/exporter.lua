LoadLib("lib/packer.lua")
LoadLib("lib/writer.lua")









Exporter = {}

function Exporter.new(sprite)
    local obj = {}
    obj.raw = sprite
    obj.images = {}
    setmetatable(obj, { __index = Exporter })
    return obj
end

function Exporter.getClassName(v)
    local mt = getmetatable(v)
    return mt.__name
end

function Exporter.findTableKey(t, f)
    for k, v in pairs(t) do
        if v == f then
            return k
        end
    end
    return nil
end

function Exporter.flattenLayers(layers)
    local r = {}

    function takeLayer(layer)
        local r = {}
        if string.match(layer.name, "^#") ~= nil then
            return r
        end
        table.insert(r, layer)
        if layer.isGroup then
            for i, l in ipairs(layer.layers) do
                for j, layer2 in ipairs(takeLayer(l)) do
                    table.insert(r, layer2)
                end
            end
        end
        return r
    end

    for i, layer in ipairs(layers) do
        for j, layer2 in ipairs(takeLayer(layer)) do
            table.insert(r, layer2)
        end
    end

    return r
end


function Exporter:registerImage(image)
    for idx, img in ipairs(self.images) do
        if img:isEqual(image) then
            return idx - 1
        end
    end
    table.insert(self.images, image)
    return #self.images - 1
end

function Exporter:export(path)
    local dir = app.fs.filePath(path)
    local prefix = string.gsub(app.fs.fileName(path), "%..+$", "")
    self.stringOffset = 1
    self.strings = {}
    self.cels = {}
    self.colliders = {}

    local w = Writer.new("PANI", 1)

    local info = w:makeChunk("INFO")
    info.misc = string.pack("I2 I2 I2", self.raw.width, self.raw.height, #self.raw.frames)

    self:exportTags(w)
    self:exportLayers(w)
    self:exportFrames(w)

    self:exportCelTable(w)
    self:exportColliderTable(w)
    self:exportImages(w, dir, prefix)
    self:exportStringTable(w)

    local f = io.open(path, 'w+')
    if f then
        f:write(w:toString())
        f:close()
    end
end

function Exporter:registerString(s)
    for i, v in ipairs(self.strings) do
        if v.string == s then
            return v.offset
        end
    end

    local offset = self.stringOffset
    table.insert(self.strings, { offset = offset, string = s })
    self.stringOffset = self.stringOffset + #s + 1

    return offset
end

function Exporter:exportStringTable(w)
    local chunk = w:makeChunk("STRG")
    local bin = string.pack("z", "")
    for i, v in ipairs(self.strings) do
        bin = bin .. string.pack("z", v.string)
    end
    chunk.data = bin
end

function Exporter:registerCel(cel)
    for i, c in ipairs(self.cels) do
        if cel.image == c.image and cel.x == c.x and cel.y == c.y and cel.w == c.w and cel.h == c.h then
            return i - 1
        end
    end
    table.insert(self.cels, cel)
    return #self.cels - 1
end

function Exporter:registerCollider(col)
    for i, c in ipairs(self.colliders) do
        if col.x == c.x and col.y == c.y and col.w == c.w and col.h == c.h then
            return i - 1
        end
    end
    table.insert(self.colliders, col)
    return #self.colliders - 1
end

function Exporter:exportCelTable(w)
    local chunk = w:makeChunk("CELS")
    local bin = ''
    for i, c in ipairs(self.cels) do
        bin = bin .. string.pack("I2 i2 i2", c.image, c.x, c.y)
    end
    chunk.data = bin
    chunk.misc = string.pack("I2", #self.cels)
end

function Exporter:exportColliderTable(w)
    local chunk = w:makeChunk("COLS")
    local bin = ''
    for i, c in ipairs(self.colliders) do
        bin = bin .. string.pack("i2 i2 I2 I2", c.x, c.y, c.w, c.h)
    end
    chunk.data = bin
    chunk.misc = string.pack("I2", #self.colliders)
end

function Exporter:exportTags(w)
    local chunk = w:makeChunk("TAGS")
    chunk.misc = string.pack("I2", #self.raw.tags)
    local tagData = {}
    for i, tag in ipairs(self.raw.tags) do
        local so = self:registerString(tag.name)
        local t = string.pack("I2 I2 I2", tag.fromFrame.frameNumber, tag.toFrame.frameNumber, so)
        table.insert(tagData, Writer.padding(t, 2))
    end
    chunk:concatData(tagData)
end

function Exporter:exportLayers(w)
    local chunk = w:makeChunk("LAYS")
    local bin = ''
    local layers = self.flattenLayers(self.raw.layers)
    chunk.misc = string.pack("I2", #layers)

    for i, layer in ipairs(layers) do
        local parent = (self.getClassName(layer.parent) == "Layer") and layer.parent or nil
        local parentIndex = (parent ~= nil) and math.tointeger(self.findTableKey(layers, parent)) - 1 or -1

        if not layer.isGroup then
            local collider = string.match(layer.name, "^@") ~= nil
            local t = collider and 'C' or 'L'
            bin = bin .. string.pack("c1 i1 I2 I2", t, parentIndex, self:registerString(layer.name), 0)
        else
            bin = bin .. string.pack("c1 i1 I2 I2", 'G', parentIndex, self:registerString(layer.name), #layer.layers)
        end
    end

    chunk.data = bin
end

function Exporter:exportFrames(w)
    local chunk = w:makeChunk("FRAM")
    chunk.misc = string.pack("I2", #self.raw.frames)

    local data = {}
    for i, frame in ipairs(self.raw.frames) do
        local bin = ''
        local duration = math.tointeger(frame.duration * 1000.0)
        assert(duration ~= nil)
        bin = bin .. string.pack("I2", duration)
        bin = bin .. self:exportFrameLayers(w, frame)
        table.insert(data, bin)
    end

    chunk:setDataAsJumpTable(2, data)
end

function Exporter:exportFrameLayers(w, frame)
    local bin = ''
    local layers = self.flattenLayers(frame.sprite.layers)
    for i, layer in ipairs(layers) do
        if not layer.isGroup then
            local collider = string.match(layer.name, "^@") ~= nil
            local cel, cb = self:exportCels(w, frame, layer.cels, collider)
            bin = bin .. string.pack("I2 i2", cb, cel)
        end
    end
    return bin
end

function Exporter:exportCels(w, frame, cels, collider)
    local outputCel = nil 
    for i, cel in ipairs(cels) do
        if cel.frameNumber == frame.frameNumber then
            if (cel.image ~= nil) and (not cel.bounds.isEmpty) and (not cel.image:isEmpty()) then
                outputCel = cel
                break
            end
        end
    end
    if outputCel == nil then
        return -1, 0
    end 
    local rc = outputCel.bounds
    local usercb = 0
    if string.len(outputCel.data) > 0 then
        usercb = self:registerString(outputCel.data)
    end
    if collider then
        local tmp = { x = rc.x, y = rc.y, w = rc.width, h = rc.height }
        return self:registerCollider(tmp), usercb
    else
        local imageIndex = self:registerImage(outputCel.image)
        local tmp = { image = imageIndex, x = rc.x, y = rc.y, w = rc.width, h = rc.height }
        return self:registerCel(tmp), usercb
    end
end

function Exporter:exportImages(w, dir, prefix)
    local rects = {}
    for i, img in ipairs(self.images) do
        local rc = Packer.Rect.new(0, 0, img.width, img.height)
        rc.object = img
        rc.originalWidth = rc.w
        rc:alignSize(8, 0)
        table.insert(rects, rc)
    end
    local packer = Packer.new()
    local width, height = packer:pack(rects, 0)

    local outputImage = Image(width, height, app.activeSprite.colorMode)
    outputImage:clear()
    for i, v in ipairs(rects) do
        outputImage:drawImage(v.object, v.x, v.y)
    end
    outputImage:saveAs(app.fs.joinPath(dir, prefix..".png"))

    local chunk = w:makeChunk("IMAG")
    chunk.misc = string.pack("I2", #self.images)
    local bin = ''
    for i, img in ipairs(self.images) do
        for i, rc in ipairs(rects) do
            if rc.object == img then
                bin = bin .. string.pack("i2 i2 I2 I2", rc.x, rc.y, rc.originalWidth, rc.h)
            end
        end
    end
    chunk.data = bin
end

-- dump
function Exporter:dump(path)
    local yaml = ''
    self.images = {}

    yaml = self:dumpInfo(yaml)
    yaml = self:dumpFrames(yaml)
    local dir = app.fs.filePath(path)
    local prefix = string.gsub(app.fs.fileName(path), "%..+$", "")
    yaml = self:dumpImages(yaml, dir, prefix)

    local f = io.open(app.fs.joinPath(dir, prefix..".log"), 'w+')
    if f then
        f:write(yaml)
        f:close()
    end

    return yaml
end

function Exporter.indent(indentLevel)
    return string.rep("    ", indentLevel)
end

function Exporter:dumpImages(yaml, dir, prefix)
    yaml = yaml .. "textures:\n"

    for i, img in ipairs(self.images) do
        yaml = yaml .. Exporter.indent(1) .. string.format("- %s_%03d.png\n", prefix, i)
    end
    return yaml
end

function Exporter:dumpInfo(yaml)
    yaml = yaml .. "info:\n"
    yaml = yaml .. Exporter.indent(1) .. string.format("size: [%d, %d]\n", self.raw.width, self.raw.height)
    yaml = yaml .. Exporter.indent(1) .. string.format("totalframe: %d\n", #self.raw.frames)
    return yaml
end

function Exporter:dumpFrames(yaml)
    local f = function(self, yaml, indent, frame)
        yaml = yaml .. Exporter.indent(indent+0) .. "-\n"
        yaml = yaml .. Exporter.indent(indent+1) .. string.format("framenumber: %d\n", frame.frameNumber)
        yaml = self:dumpLayers(yaml, indent+1, frame)
        return yaml
    end
    yaml = yaml .. "frames:\n"
    for i, frame in ipairs(self.raw.frames) do
        yaml = f(self, yaml, 1, frame)
    end
    return yaml
end

function Exporter:dumpLayers(yaml, indent, frame)
    yaml = yaml .. Exporter.indent(indent) .. "layers:\n"
    for i, layer in ipairs(frame.sprite.layers) do
        yaml = self:dumpLayer(yaml, indent+1, frame, layer)
    end
    return yaml
end

function Exporter:dumpLayer(yaml, indent, frame, layer)
    yaml = yaml .. Exporter.indent(indent) .. "-\n"
    yaml = yaml .. Exporter.indent(indent+1) .. string.format("name: %s\n", layer.name)
    if not layer.isGroup then
        yaml = self:dumpCels(yaml, indent+1, frame, layer.cels)
    else
        yaml = yaml .. Exporter.indent(indent+1) .. "sub:\n"
        for i, sub in ipairs(layer.layers) do
            yaml = self:dumpLayer(yaml, indent+2, frame, sub)
        end
    end
    return yaml
end

function Exporter:dumpCels(yaml, indent, frame, cels)
    local outputCels = {}
    for i, cel in ipairs(cels) do
        if cel.frameNumber == frame.frameNumber then
            if (cel.image ~= nil) and (not cel.bounds.isEmpty) and (not cel.image:isEmpty()) then
                table.insert(outputCels, cel)
            end
        end
    end
    if #outputCels == 0 then
        yaml = yaml .. Exporter.indent(indent) .. "cels: []\n"
    else
        yaml = yaml .. Exporter.indent(indent) .. "cels:\n"
        for i, cel in ipairs(outputCels) do
            local rc = cel.bounds
            local img = cel.image
            local imgIndex = self:registerImage(img)
            yaml = yaml .. Exporter.indent(indent+1) .. string.format("- [%d, %d, %d, %d, %d]\n", imgIndex, rc.x, rc.y, rc.width, rc.height)
        end
    end
    return yaml
end
