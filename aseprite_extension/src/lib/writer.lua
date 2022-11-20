Writer = {}
Writer.Chunk = {}

function Writer.new(id, version)
    local obj = {}
    setmetatable(obj, { __index = Writer })
    obj.id = id
    obj.version = version
    obj.chunks = {}
    return obj
end

function Writer:toString()
    local bin = self:makeHeader()
    local offset = #bin

    for i = 1, #self.chunks - 1 do
        offset = offset + #self.chunks[i]:toString()
        self.chunks[i].next = offset
    end
    for i, c in ipairs(self.chunks) do
        bin = bin .. c:toString()
    end

    return bin
end

function Writer:makeHeader()
    return string.pack("c4 I4 xxxx xxxx", self.id, self.version)
end

function Writer:makeChunk(id)
    local chunk = Writer.Chunk.new(id)
    table.insert(self.chunks, chunk)
    return chunk
end

function Writer.Chunk.new(id)
    local obj = {}
    setmetatable(obj, { __index = Writer.Chunk })
    obj.id = id
    obj.misc = ''
    obj.data = ''
    obj.next = 0
    return obj
end

function Writer.Chunk:toString()
    return string.pack("c4 I2 I2 c8", self.id, #self.data, self.next >> 4, self.misc) .. Writer.padding(self.data, 16)
end

function Writer.Chunk:concatData(list)
    local bin = ''
    for i, data in ipairs(list) do
        bin = bin .. data
    end
    self.data = bin
end

function Writer.Chunk:setDataAsJumpTable(jumpTableSize, bodies)
    local bin = ''

    local offset = jumpTableSize * #bodies
    for i, data in ipairs(bodies) do
        bin = bin .. string.pack(string.format("I%d", jumpTableSize), offset)
        offset = offset + #data
    end

    for i, data in ipairs(bodies) do
        bin = bin .. data
    end

    self.data = bin
end

function Writer.padding(bin, align)
    if #bin == 0 then
        return bin
    end
    local len = math.tointeger((#bin + align - 1) / align) * align
    return bin .. string.pack(string.rep("x", len - #bin), 0)
end
