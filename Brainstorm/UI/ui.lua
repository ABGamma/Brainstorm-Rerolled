-- ui.lua — adds the "Brainstorm-Rerolled" tab to the game's Settings menu.
--
-- The tab shows which saved filter auto-reroll uses ("ACTIVE FILTER") and a
-- button to manage filters. Its contents are built by
-- Brainstorm.FB.settings_tab() in UI/filter_builder.lua. (The old built-in
-- filter options were replaced by saved filters; see filter_builder.lua.)

-- When true, the next Settings menu opens on the Brainstorm-Rerolled tab (used when
-- coming back from the filter screens).
Brainstorm.open_settings_tab = false

-- The Settings menu is the create_tabs call with tab_h = 7.05.
local create_tabs_ref = create_tabs
function create_tabs(args)
  if args and args.tab_h == 7.05 then
    local chosen = Brainstorm.open_settings_tab
    Brainstorm.open_settings_tab = false
    if chosen then
      -- create_tabs shows the tab marked `chosen`.
      for _, tab in ipairs(args.tabs) do
        tab.chosen = false
      end
    end
    args.tabs[#args.tabs + 1] = {
      label = "Brainstorm-Rerolled",
      chosen = chosen,
      tab_definition_function = function()
        return Brainstorm.FB.settings_tab()
      end,
      tab_definition_function_args = "Brainstorm",
    }
  end
  return create_tabs_ref(args)
end
