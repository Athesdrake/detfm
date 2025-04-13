function format_const(id)
    return string.format("lua_const_%03d", id)
end

function format_method(id)
    return string.format("lua_method_%03d", id)
end

function format_function(id)
    return string.format("lua_function_%03d", id)
end

function format_vars(id)
    return string.format("lua_vars_%03d", id)
end

function format_class(id)
    return string.format("lua_class_%03d", id)
end

function format_error(id)
    return string.format("lua_error%d", id)
end

function format_symbol(id)
    return string.format("lua_ClassSymbol_%d", id)
end

function packet_prefix(side)
    if side == "clientbound" then
        return "SPacket"
    end
    if side == "serverbound" then
        return "CPacket"
    end
    if side == "tribulle_clientbound" then
        return "TCPacket_"
    end
    if side == "tribulle_serverbound" then
        return "TSPacket_"
    end
end

function format_packet(side, categ_id, pkt_id, name)
    local prefix = packet_prefix(side)
    if side:sub(1, #"tribulle") == "tribulle" then
        local pkt_id = pkt_id + (categ_id * 256)
        return string.format("lua_%s%04x%s", prefix, pkt_id, name)
    end
    return string.format("lua_%s%02x%02x%s", prefix, categ_id, pkt_id, name)
end

function format_subhandler(id)
    return string.format("lua_PacketSubHandler_%02x", id)
end

function format_unknown_packet(id)
    return string.format("lua_CPacket_u%02x", id)
end
