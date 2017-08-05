function split_string(str, token)
    local t = {};
    while #str > 0 do
        local pos = str:find(token);
        if not pos then
            t[#t + 1] = str;
            break;
        end

        if pos > 1 then
            t[#t + 1] = str:sub(1, pos - 1);
        end
        str = str:sub(pos + 1, #str);
    end
    return t;
end


