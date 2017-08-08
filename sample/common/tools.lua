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

--函数装饰器: 保护性的调用指定函数,如果出错则写日志
--主要用于一些C回调函数,它们本身不写错误日志
--通过这个装饰器,方便查错
function log_decorator(proc)
    return function(...)
        local ok, err = xpcall(proc, debug.traceback, ...);
        if not ok then
            log_err("%s", err);
        end
    end
end


