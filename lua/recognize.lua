local settings = {
    shortlist_size = 24,
    weights = { colour = 0.60, detail = 0.22, edge = 0.13, aspect = 0.05 },
    maximum_error = 0.070,
    absolute_ambiguity_gap = 0.004,
    relative_ambiguity_gap = 0.20,
}


local answer_names = {}

local tie_order = { "no", "fr", "au", "fi", "nl", "nz", "gb", "us" }
local tie_priority = {}
for index, code in ipairs(tie_order) do
    tie_priority[code] = index
end

local function score(measurement)
    local weights = settings.weights
    return weights.colour * measurement.colour
         + weights.detail * measurement.detail
         + weights.edge * measurement.edge
         + weights.aspect * measurement.aspect
end

local function recognize()
    local image = native.describe()
    local measurements = native.compare(settings.shortlist_size)
    local best_by_code = {}

    for _, measurement in ipairs(measurements) do
        local candidate = {
            code = measurement.code,
            name = answer_names[measurement.code] or measurement.name,
            error = score(measurement),
        }
        local previous = best_by_code[candidate.code]
        if not previous or candidate.error < previous.error then
            best_by_code[candidate.code] = candidate
        end
    end

    local ranked = {}
    for _, candidate in pairs(best_by_code) do
        ranked[#ranked + 1] = candidate
    end
    table.sort(ranked, function(first, second)
        if first.error ~= second.error then
            return first.error < second.error
        end
        local first_priority = tie_priority[first.code] or 1000
        local second_priority = tie_priority[second.code] or 1000
        if first_priority ~= second_priority then
            return first_priority < second_priority
        end
        return first.code < second.code
    end)

    if #ranked == 0 then
        error("The reference collection is empty.")
    end

    local insufficient_detail = image.width < 12 or image.height < 8 or image.variation < 0.008
    if insufficient_detail or ranked[1].error > settings.maximum_error then
        local suggestions = {}
        for index = 1, math.min(3, #ranked) do
            suggestions[index] = ranked[index]
        end
        return { status = "uncertain", candidates = suggestions }
    end

    local ambiguity_gap = settings.absolute_ambiguity_gap
        + ranked[1].error * settings.relative_ambiguity_gap
    local close_matches = {}
    for _, candidate in ipairs(ranked) do
        if candidate.error > ranked[1].error + ambiguity_gap then
            break
        end
        close_matches[#close_matches + 1] = candidate
    end

    return {
        status = #close_matches > 1 and "ambiguous" or "match",
        candidates = close_matches,
    }
end

return { recognize = recognize }
