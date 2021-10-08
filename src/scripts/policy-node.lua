-- WirePlumber
--
-- Copyright © 2020 Collabora Ltd.
--    @author Julian Bouzas <julian.bouzas@collabora.com>
--
-- SPDX-License-Identifier: MIT

-- Receive script arguments from config.lua
local config = ...

-- ensure config.move and config.follow are not nil
config.move = config.move or false
config.follow = config.follow or false

local pending_rescan = false

function parseBool(var)
  return var and (var == "true" or var == "1")
end

function createLink (si, si_target)
  local out_item = nil
  local in_item = nil

  if si.properties["item.node.direction"] == "output" then
    -- playback
    out_item = si
    in_item = si_target
  else
    -- capture
    in_item = si
    out_item = si_target
  end

  Log.info (string.format("link %s <-> %s",
      tostring(si.properties["node.name"]),
      tostring(si_target.properties["node.name"])))

  -- create and configure link
  local si_link = SessionItem ( "si-standard-link" )
  if not si_link:configure {
    ["out.item"] = out_item,
    ["in.item"] = in_item,
    ["out.item.port.context"] = "output",
    ["in.item.port.context"] = "input",
    ["is.policy.item.link"] = true,
  } then
    Log.warning (si_link, "failed to configure si-standard-link")
    return
  end

  -- register
  si_link:register ()

  -- activate
  si_link:activate (Feature.SessionItem.ACTIVE, function (l, e)
    if e ~= nil then
      Log.warning (l, "failed to activate si-standard-link")
      si_link:remove ()
    else
      Log.info (l, "activated si-standard-link")
    end
  end)
end

function canLink (properties, si_target)
  local target_properties = si_target.properties

  -- nodes must have the same media type
  if properties["media.type"] ~= target_properties["media.type"] then
    return false
  end

  -- nodes must have opposite direction, or otherwise they must be both input
  -- and the target must have a monitor (so the target will be used as a source)
  local function isMonitor(properties)
    return properties["item.node.direction"] == "input" and
          parseBool(properties["item.features.monitor"]) and
          not parseBool(properties["item.features.no-dsp"]) and
          properties["item.factory.name"] == "si-audio-adapter"
  end

  if properties["item.node.direction"] == target_properties["item.node.direction"]
      and not isMonitor(target_properties) then
    return false
  end

  -- check link group
  local function canLinkGroupCheck (link_group, si_target, hops)
    local target_props = si_target.properties
    local target_link_group = target_props["node.link-group"]

    if hops == 8 then
      return false
    end

    -- allow linking if target has no link-group property
    if not target_link_group then
      return true
    end

    -- do not allow linking if target has the same link-group
    if link_group == target_link_group then
      return false
    end

    -- make sure target is not linked with another node with same link group
    -- start by locating other nodes in the target's link-group, in opposite direction
    for n in linkables_om:iterate {
      Constraint { "id", "!", si_target.id, type = "gobject" },
      Constraint { "item.node.direction", "!", target_props["item.node.direction"] },
      Constraint { "node.link-group", "=", target_link_group },
    } do
      -- iterate their peers and return false if one of them cannot link
      for silink in links_om:iterate() do
        local out_id = tonumber(silink.properties["out.item.id"])
        local in_id = tonumber(silink.properties["in.item.id"])
        if out_id == n.id or in_id == n.id then
          local peer_id = (out_id == n.id) and in_id or out_id
          local peer = linkables_om:lookup {
            Constraint { "id", "=", peer_id, type = "gobject" },
          }
          if peer and not canLinkGroupCheck (link_group, peer, hops + 1) then
            return false
          end
        end
      end
    end
    return true
  end

  local link_group = properties["node.link-group"]
  if link_group then
    return canLinkGroupCheck (link_group, si_target, 0)
  end
  return true
end

-- Try to locate a valid target node that was explicitly defined by the user
-- Use the target.node metadata, if config.move is enabled,
-- then use the node.target property that was set on the node
-- `properties` must be the properties dictionary of the session item
-- that is currently being handled
function findDefinedTarget (properties)
  local function findTargetByTargetNodeMetadata (properties)
    local node_id = properties["node.id"]
    local metadata = metadata_om:lookup()
    local target_id = metadata and metadata:find(node_id, "target.node") or nil
    if target_id and tonumber(target_id) > 0 then
      local si_target = linkables_om:lookup {
        Constraint { "node.id", "=", target_id },
      }
      if si_target and canLink (properties, si_target) then
        return si_target
      end
    end
    return nil
  end

  local function findTargetByNodeTargetProperty (properties)
    local target_id = properties["node.target"]
    if target_id then
      for si_target in linkables_om:iterate() do
        local target_props = si_target.properties
        if (target_props["node.id"] == target_id or
            target_props["node.name"] == target_id or
            target_props["object.path"] == target_id) and
            canLink (properties, si_target) then
          return si_target
        end
      end
    end
    return nil
  end

  return (config.move and findTargetByTargetNodeMetadata (properties) or nil)
      or findTargetByNodeTargetProperty (properties)
end

function getTargetDirection(properties)
  local target_direction = nil
  if properties["item.node.direction"] == "output" or
     (properties["item.node.direction"] == "input" and
        parseBool(properties["stream.capture.sink"])) then
    target_direction = "input"
  else
    target_direction = "output"
  end
  return target_direction
end

function getDefaultNode(properties, target_direction)
  local target_media_class =
        properties["media.type"] ..
        (target_direction == "input" and "/Sink" or "/Source")
  return default_nodes:call("get-default-node", target_media_class)
end

-- Try to locate a valid target node that was NOT explicitly defined by the user
-- `properties` must be the properties dictionary of the session item
-- that is currently being handled
function findUndefinedTarget (properties)
  local function findTargetByDefaultNode (properties, target_direction)
    local def_id = getDefaultNode(properties, target_direction)
    if def_id ~= Id.INVALID then
      local si_target = linkables_om:lookup {
        Constraint { "node.id", "=", def_id },
      }
      if si_target and canLink (properties, si_target) then
        return si_target
      end
    end
    return nil
  end

  local function findTargetByFirstAvailable (properties, target_direction)
    for si_target in linkables_om:iterate {
      Constraint { "item.node.type", "=", "device" },
      Constraint { "item.node.direction", "=", target_direction },
      Constraint { "media.type", "=", properties["media.type"] },
    } do
      if canLink (properties, si_target) then
        return si_target
      end
    end
    return nil
  end

  local target_direction = getTargetDirection(properties)
  return findTargetByDefaultNode (properties, target_direction)
      or findTargetByFirstAvailable (properties, target_direction)
end

function getSiLinkAndSiPeer (si, si_props)
  local self_id_key = (si_props["item.node.direction"] == "output") and
                      "out.item.id" or "in.item.id"
  local peer_id_key = (si_props["item.node.direction"] == "output") and
                      "in.item.id" or "out.item.id"
  local silink = links_om:lookup { Constraint { self_id_key, "=", si.id } }
  if silink then
    local peer_id = tonumber(silink.properties[peer_id_key])
    local peer = linkables_om:lookup {
      Constraint { "id", "=", peer_id, type = "gobject" },
    }
    return silink, peer
  end
  return nil, nil
end

function checkLinkable(si)
  -- only handle stream session items
  local si_props = si.properties
  if not si_props or si_props["item.node.type"] ~= "stream" then
    return false
  end

  -- Determine if we can handle item by this policy
  local media_role = si_props["media.role"]
  if endpoints_om:get_n_objects () > 0 and media_role ~= nil then
    return false
  end

  return true, si_props
end

function handleLinkable (si)
  local valid, si_props = checkLinkable(si)
  if not valid then
    return
  end

  -- check if we need to link this node at all
  local autoconnect = parseBool(si_props["node.autoconnect"])
  if not autoconnect then
    Log.debug (si, tostring(si_props["node.name"]) .. " does not need to be autoconnected")
    return
  end

  Log.info (si, "handling item: " .. tostring(si_props["node.name"]))

  -- get reconnect
  local reconnect = not parseBool(si_props["node.dont-reconnect"])

  -- find target
  local si_target = findDefinedTarget (si_props)
  if not si_target and not reconnect then
    Log.info (si, "... destroy node")
    local node = si:get_associated_proxy ("node")
    node:request_destroy()
    return
  elseif not si_target and reconnect then
    si_target = findUndefinedTarget (si_props)
  end
  if not si_target then
    Log.info (si, "... target not found")
    return
  end

  -- Check if item is linked to proper target, otherwise re-link
  local si_link, si_peer = getSiLinkAndSiPeer (si, si_props)
  if si_link then
    if si_peer and si_peer.id == si_target.id then
      Log.debug (si, "... already linked to proper target")
      return
    end

    -- only remove old link if active, otherwise schedule rescan
    if ((si_link:get_active_features() & Feature.SessionItem.ACTIVE) ~= 0) then
      si_link:remove ()
      Log.info (si, "... moving to new target")
    else
      pending_rescan = true
      Log.info (si, "... scheduled rescan")
      return
    end
  end

  -- create new link
  createLink (si, si_target)
end

function unhandleLinkable (si)
  local valid, si_props = checkLinkable(si)
  if not valid then
    return
  end

  Log.info (si, "unhandling item: " .. tostring(si_props["node.name"]))

  -- remove any links associated with this item
  for silink in links_om:iterate() do
    local out_id = tonumber (silink.properties["out.item.id"])
    local in_id = tonumber (silink.properties["in.item.id"])
    if out_id == si.id or in_id == si.id then
      silink:remove ()
      Log.info (silink, "... link removed")
    end
  end
end

function rescan()
  for si in linkables_om:iterate() do
    handleLinkable (si)
  end

  -- if pending_rescan, re-evaluate after sync
  if pending_rescan then
    pending_rescan = false
    Core.sync (function (c)
      rescan()
    end)
  end
end

default_nodes = Plugin.find("default-nodes-api")

metadata_om = ObjectManager {
  Interest {
    type = "metadata",
    Constraint { "metadata.name", "=", "default" },
  }
}

endpoints_om = ObjectManager { Interest { type = "SiEndpoint" } }

linkables_om = ObjectManager {
  Interest {
    type = "SiLinkable",
    -- only handle si-audio-adapter and si-node
    Constraint { "item.factory.name", "c", "si-audio-adapter", "si-node" },
  }
}

links_om = ObjectManager {
  Interest {
    type = "SiLink",
    -- only handle links created by this policy
    Constraint { "is.policy.item.link", "=", true },
  }
}

function cleanupTargetNodeMetadata()
  local metadata = metadata_om:lookup()
  if metadata then
    local to_remove = {}
    for s, k, t, v in metadata:iterate(Id.ANY) do
      if k == "target.node" then
        if v == "-1" then
          -- target.node == -1 is useless, it means the default node
          table.insert(to_remove, s)
        else
          -- if the target.node value is the same as the default node
          -- that would be selected for this stream, remove it
          local si = linkables_om:lookup { Constraint { "node.id", "=", s } }
          local properties = si.properties
          local def_id = getDefaultNode(properties, getTargetDirection(properties))
          if tostring(def_id) == v then
            table.insert(to_remove, s)
          end
        end
      end
    end

    for _, s in ipairs(to_remove) do
      metadata:set(s, "target.node", nil, nil)
    end
  end
end

-- listen for default node changes if config.follow is enabled
if config.follow then
  default_nodes:connect("changed", function ()
    cleanupTargetNodeMetadata()
    rescan()
  end)
end

-- listen for target.node metadata changes if config.move is enabled
if config.move then
  metadata_om:connect("object-added", function (om, metadata)
    metadata:connect("changed", function (m, subject, key, t, value)
      if key == "target.node" then
        rescan()
      end
    end)
  end)
end

linkables_om:connect("objects-changed", function (om)
  rescan()
end)

linkables_om:connect("object-removed", function (om, si)
  unhandleLinkable (si)
end)

metadata_om:activate()
endpoints_om:activate()
linkables_om:activate()
links_om:activate()
