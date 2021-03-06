local running = true

local width, height = gpu.width, gpu.height

local gw, gh = math.floor(width / 5 - 2), math.floor(height / 5 - 2)

if gw % 2 ~= 0 then
  gw = gw - 1
end

if gh % 2 ~= 0 then
  gh = gh - 1
end

local grid = {}
for i = 1, gw do
  grid[i] = {}
  for j = 1, gh do
    grid[i][j] = 0
  end
end
for i = 1, 3 do
  grid[gw / 2][gh / 2 + i - 1] = 1
end
while true do
  local x, y = math.random(1, gw), math.random(1, gh)
  if grid[x][y] == 0 then
    grid[x][y] = 5
    break
  end
end

local head = {gw / 2, gh / 2}
local tail = {gw / 2, gh / 2 + 2}
local dir  = 1

local cyc = 1
local slowdown = 2
local function update()
  cyc = cyc % slowdown + 1
  if cyc == 1 then
    local hp = dir
    grid[head[1]][head[2]] = dir
    if hp == 1 then
      head = {head[1], head[2] - 1}
    elseif hp == 2 then
      head = {head[1] + 1, head[2]}
    elseif hp == 3 then
      head = {head[1], head[2] + 1}
    else
      head = {head[1] - 1, head[2]}
    end
    local rp = grid[head[1]][head[2]]
    grid[head[1]][head[2]] = dir

    if rp ~= 5 then
      local sp = grid[tail[1]][tail[2]]
      grid[tail[1]][tail[2]] = 0
      tail[1] = tail[1] + (sp % 2 == 0 and 3 - sp or 0)
      tail[2] = tail[2] + ((sp % 2 - 1) == 0 and sp - 2 or 0)
    else
      while true do
        local x, y = math.random(1, gw), math.random(gh)
        if grid[x][y] == 0 then
          grid[x][y] = 5
          break
        end
      end
    end
  end
end

local function draw()
  for i = 1, gw do
    for j = 1, gh do
      if grid[i][j] > 0 then
        gpu.drawRectangle(i * 5, j * 5, 4, 4, grid[i][j] == 5 and 8 or 12)
      end
    end
  end

  --gpu.drawRectangle(tail[1] * 5, tail[2] * 5, 4, 4, 9)
  --gpu.drawRectangle(head[1] * 5, head[2] * 5, 4, 4, 4)
end

local function processEvent(e, ...)
  local args = {...}

  if e == "key" then
    local k = args[1]
    if k == "escape" then
      running = false
    elseif k == "up" then
      dir = 1
    elseif k == "right" then
      dir = 2
    elseif k == "down" then
      dir = 3
    elseif k == "left" then
      dir = 4
    end
  end
end

local eventQueue = {}
while running do
  while true do
    local e = {coroutine.yield()}
    if #e == 0 then break end
    eventQueue[#eventQueue + 1] = e
  end

  while #eventQueue > 0 do
    processEvent(unpack(
      table.remove(eventQueue, 1)))
  end

  update()

  gpu.clear(13)

  draw()

  gpu.swap()
end
