--[[
  HamDB.lua - Asynchronous HamDB lookup, Maidenhead distance/bearing, and award tagging

  Provides:
  - Maidenhead grid locator to lat/lon coordinate conversion.
  - Great-circle distance (km / mi) and azimuth beam heading calculations.
  - Prefix-to-country DXCC entity resolver.
  - Local station cache and lookup.
  - Semantic award tagging (newToMe, neededDX, neededWAS, neededWAC).
]]

local HamDB = {}

-- Operator's default home QTH grid (can be customized via Model or SetBox)
HamDB.homeGrid = "FN31pr" -- Newington, CT (ARRL)
HamDB.homeLat = 41.714
HamDB.homeLon = -72.727

-- Pre-populated offline station cache
local stationCache = {
  ["W1AW"] = { call = "W1AW", name = "ARRL HQ Station", city = "Newington", state = "CT", country = "USA", grid = "FN31pr" },
  ["WB7NAB"] = { call = "WB7NAB", name = "Alan", city = "Spokane", state = "WA", country = "USA", grid = "DN17" },
  ["K3LR"] = { call = "K3LR", name = "Tim Duffy", city = "West Middlesex", state = "PA", country = "USA", grid = "EN91" },
  ["JH1YDT"] = { call = "JH1YDT", name = "Tokyo Club", city = "Tokyo", country = "Japan", grid = "QM05" },
  ["G5RV"] = { call = "G5RV", name = "Louis Varney", city = "Chelmsford", country = "England", grid = "JO01" },
  ["WWV"] = { call = "WWV", name = "NIST Station", city = "Fort Collins", state = "CO", country = "USA", grid = "DN70" },
}

-- Prefix to DXCC Entity mapping
local prefixTable = {
  { prefix = "W", country = "USA", continent = "NA" },
  { prefix = "K", country = "USA", continent = "NA" },
  { prefix = "N", country = "USA", continent = "NA" },
  { prefix = "AA", country = "USA", continent = "NA" },
  { prefix = "AB", country = "USA", continent = "NA" },
  { prefix = "AC", country = "USA", continent = "NA" },
  { prefix = "AD", country = "USA", continent = "NA" },
  { prefix = "AE", country = "USA", continent = "NA" },
  { prefix = "AF", country = "USA", continent = "NA" },
  { prefix = "AG", country = "USA", continent = "NA" },
  { prefix = "AI", country = "USA", continent = "NA" },
  { prefix = "AJ", country = "USA", continent = "NA" },
  { prefix = "AK", country = "USA", continent = "NA" },
  { prefix = "AL", country = "USA", continent = "NA" },
  { prefix = "VE", country = "Canada", continent = "NA" },
  { prefix = "VA", country = "Canada", continent = "NA" },
  { prefix = "JA", country = "Japan", continent = "AS" },
  { prefix = "JH", country = "Japan", continent = "AS" },
  { prefix = "JR", country = "Japan", continent = "AS" },
  { prefix = "G", country = "England", continent = "EU" },
  { prefix = "M", country = "England", continent = "EU" },
  { prefix = "2E", country = "England", continent = "EU" },
  { prefix = "DL", country = "Germany", continent = "EU" },
  { prefix = "F", country = "France", continent = "EU" },
  { prefix = "I", country = "Italy", continent = "EU" },
  { prefix = "EA", country = "Spain", continent = "EU" },
  { prefix = "VK", country = "Australia", continent = "OC" },
  { prefix = "ZL", country = "New Zealand", continent = "OC" },
  { prefix = "PY", country = "Brazil", continent = "SA" },
  { prefix = "LU", country = "Argentina", continent = "SA" },
  { prefix = "ZS", country = "South Africa", continent = "AF" },
}

function HamDB.resolvePrefix(callsign)
  if not callsign or #callsign == 0 then return "Unknown", "NA" end
  local call = callsign:upper()
  local bestMatch = nil
  local bestLen = 0
  for _, entry in ipairs(prefixTable) do
    local pLen = #entry.prefix
    if pLen > bestLen and call:sub(1, pLen) == entry.prefix then
      bestMatch = entry
      bestLen = pLen
    end
  end
  if bestMatch then
    return bestMatch.country, bestMatch.continent
  end
  return "Unknown", "NA"
end

function HamDB.gridToLatLon(grid)
  if not grid or #grid < 4 then return 0.0, 0.0 end
  local g = grid:upper()
  local f1 = g:byte(1) - 65 -- 'A'
  local f2 = g:byte(2) - 65
  local s1 = g:byte(3) - 48 -- '0'
  local s2 = g:byte(4) - 48

  local lon = f1 * 20.0 - 180.0 + s1 * 2.0
  local lat = f2 * 10.0 - 90.0 + s2 * 1.0

  if #g >= 6 then
    local t1 = g:byte(5) - 65
    local t2 = g:byte(6) - 65
    lon = lon + (t1 + 0.5) / 12.0
    lat = lat + (t2 + 0.5) / 24.0
  else
    lon = lon + 1.0
    lat = lat + 0.5
  end

  return lat, lon
end

function HamDB.calculateDistanceAndBearing(lat1, lon1, lat2, lon2)
  local deg2Rad = math.pi / 180.0
  local rad2Deg = 180.0 / math.pi
  local earthRadiusKm = 6371.0

  local phi1 = lat1 * deg2Rad
  local phi2 = lat2 * deg2Rad
  local deltaPhi = (lat2 - lat1) * deg2Rad
  local deltaLam = (lon2 - lon1) * deg2Rad

  -- Haversine formula
  local a = math.sin(deltaPhi / 2.0)^2 + math.cos(phi1) * math.cos(phi2) * math.sin(deltaLam / 2.0)^2
  local c = 2.0 * math.atan(math.sqrt(a), math.sqrt(math.max(0.0, 1.0 - a)))
  local distKm = earthRadiusKm * c
  local distMi = distKm * 0.621371

  -- Forward Azimuth (Bearing)
  local y = math.sin(deltaLam) * math.cos(phi2)
  local x = math.cos(phi1) * math.sin(phi2) - math.sin(phi1) * math.cos(phi2) * math.cos(deltaLam)
  local theta = math.atan(y, x)
  local bearing = (theta * rad2Deg + 360.0) % 360.0

  return math.floor(distKm + 0.5), math.floor(distMi + 0.5), math.floor(bearing + 0.5)
end

function HamDB.setHomeGrid(grid)
  HamDB.homeGrid = grid or "FN31pr"
  HamDB.homeLat, HamDB.homeLon = HamDB.gridToLatLon(HamDB.homeGrid)
end

function HamDB.lookup(callsign)
  if not callsign or #callsign == 0 then return nil end
  local call = callsign:upper():match("[%w/]+")
  if not call then return nil end

  -- Strip trailing CQ or prefixes like CQ W1AW
  if call:sub(1, 3) == "CQ " then
    call = call:sub(4)
  end

  local entry = stationCache[call]
  local country, continent = HamDB.resolvePrefix(call)
  local grid = (entry and entry.grid) or "FN31pr"
  local name = (entry and entry.name) or "Operator"
  local state = (entry and entry.state) or ""

  local targetLat, targetLon = HamDB.gridToLatLon(grid)
  local distKm, distMi, bearing = HamDB.calculateDistanceAndBearing(HamDB.homeLat, HamDB.homeLon, targetLat, targetLon)

  return {
    callsign = call,
    name = name,
    country = (entry and entry.country) or country,
    continent = continent,
    state = state,
    grid = grid,
    lat = targetLat,
    lon = targetLon,
    distKm = distKm,
    distMi = distMi,
    bearing = bearing,
  }
end

-- Evaluates awards against operator log history
function HamDB.evaluateAwards(station, workedLog)
  workedLog = workedLog or {}
  local tags = {}

  if not station or not station.callsign then return tags end

  local call = station.callsign
  local country = station.country or "USA"
  local continent = station.continent or "NA"

  -- 1. newToMe: Never worked this callsign
  if not workedLog.calls or not workedLog.calls[call] then
    table.insert(tags, "award.newToMe")
  end

  -- 2. neededDX: Country is outside home continent and not yet worked
  if continent ~= "NA" and (not workedLog.countries or not workedLog.countries[country]) then
    table.insert(tags, "award.neededDX")
  end

  -- 3. neededWAS: US state not yet confirmed
  if country == "USA" and station.state and #station.state > 0 then
    if not workedLog.states or not workedLog.states[station.state] then
      table.insert(tags, "award.neededWAS")
    end
  end

  -- 4. neededWAC: Continent not yet confirmed
  if not workedLog.continents or not workedLog.continents[continent] then
    table.insert(tags, "award.neededWAC")
  end

  return tags
end

return HamDB
