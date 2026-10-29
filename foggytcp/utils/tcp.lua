-- Copyright (C) 2024 Carnegie Mellon University and 
-- Hong Kong University of Science and Technology

-- This repository is used for the Computer Networks (ELEC 3120) 
-- course taught at Hong Kong University of Science 
-- and Technology.

-- No part of the project may be copied and/or 
-- distributed without the express permission of 
-- the course staff. Everyone is prohibited from 
-- releasing their forks in any public places.

tcp = Proto("foggytcp", "Foggy TCP")

local f_identifier  = ProtoField.uint32("foggytcp.identifier", "Identifier")
local f_source_port  = ProtoField.uint16("foggytcp.source_port", "Source Port")
local f_destination_port  = ProtoField.uint16("foggytcp.destination_port", "Destination Port")
local f_seq_num  = ProtoField.uint32("foggytcp.seq_num", "Sequence Number")
local f_ack_num  = ProtoField.uint32("foggytcp.ack_num", "ACK Number")
local f_hlen  = ProtoField.uint16("foggytcp.hlen", "Header Length")
local f_plen  = ProtoField.uint16("foggytcp.plen", "Packet Length")
local f_flags  = ProtoField.uint8("foggytcp.flags", "Flags")
local f_advertised_window  = ProtoField.uint16("foggytcp.advertised_window", "Advertised Window")
local f_extension_length  = ProtoField.uint16("foggytcp.extension_length", "Extension Length")
local f_extension_data = ProtoField.string("foggytcp.extension_data", "Extension Data")

tcp.fields = { f_identifier, f_source_port, f_destination_port, f_seq_num, f_ack_num, f_hlen, f_plen, f_flags, f_advertised_window, f_extension_length , f_extension_data}

function tcp.dissector(tvb, pInfo, root) -- Tvb, Pinfo, TreeItem
   if (tvb:len() ~= tvb:reported_len()) then
      return 0 -- ignore partially captured packets
      -- this can/may be re-enabled only for unfragmented UDP packets
   end

   local t = root:add(tcp, tvb(0,25))
   t:add(f_identifier, tvb(0,4))
   t:add(f_source_port, tvb(4,2))
   t:add(f_destination_port, tvb(6,2))
   t:add(f_seq_num, tvb(8,4))
   t:add(f_ack_num, tvb(12,4))
   t:add(f_hlen, tvb(16,2))
   t:add(f_plen, tvb(18,2))
   local f = t:add(f_flags, tvb(20,1))
   t:add(f_advertised_window, tvb(21,2))
   t:add(f_extension_length, tvb(23,2))
   local extension_length = tvb(23,1):int()
   t:add(f_extension_data, tvb(25,extension_length))


   local flag = tvb(20,1):uint()

   if bit.band(flag, 8) ~= 0 then
      f:add(tvb(20,1), "SYN")
   end
   if bit.band(flag, 4) ~= 0 then
      f:add(tvb(20,1), "ACK")
   end
   if bit.band(flag, 2) ~= 0 then
      f:add(tvb(20,1), "FIN")
   end

   pInfo.cols.protocol = "Foggy TCP"
end

-- have to put the port for the server here
local udpDissectorTable = DissectorTable.get("udp.port")
udpDissectorTable:add("3120", tcp)

io.stderr:write("tcp.lua is successfully loaded\n")
