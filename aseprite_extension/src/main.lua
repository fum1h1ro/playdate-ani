function LoadLib(filename)
    local extention_path = app.fs.joinPath(app.fs.userConfigPath, "extensions/playdate-ani-exporter")
    local path = app.fs.normalizePath(app.fs.joinPath(extention_path, filename))
    dofile(path)
end

function OutputFile(filename, log)
    filename = app.fs.normalizePath(filename)
    local exp = Exporter.new(app.activeSprite)
    exp:export(filename)
    if log then
        exp:dump(filename)
    end
end

if app.params['output'] ~= nil and app.activeSprite ~= nil then
    print("Batch export: "..app.params["output"])
    LoadLib("lib/exporter.lua")
    OutputFile(app.params['output'])
    return
end

local Plugin

function Execute()
    local dialog = Dialog({
        title = "Export .ani File",
    })
    local path = app.fs.filePath(app.activeSprite.filename)
    local fname = app.fs.fileTitle(app.activeSprite.filename)

    dialog
        :file({
            id = "savedialog",
            label = "Export File",
            title = "Export File",
            open = false,
            save = true,
            filename = app.fs.joinPath(path, fname..".ani"),
            filetypes = { "ani" },
            onchange = function()
                if string.len(dialog.data.savedialog) > 0 then
                    dialog:modify({
                        id = "ok",
                        enabled = true
                    })
                else
                    dialog:modify({
                        id = "ok",
                        enabled = false
                    })
                end
            end
        })
        :check({
            id = "outputlog",
            text = "Output Log",
            selected = Plugin.preferences.output_log
        })
        :button({
            id = "cancel",
            text = "Cancel",
            onclick = function()
                dialog:close()
            end
        })
        :button({
            id = "ok",
            text = "OK",
            onclick = function()
                dialog:close()
            end
        })
        :modify({
            id = "ok",
            enabled = false
        })
        :show()

    if not dialog.data.ok then
        return
    end

    Plugin.preferences.output_log = dialog.data.outputlog

    local filename = dialog.data.savedialog

    if string.len(filename) > 0 then
        OutputFile(filename, dialog.data.outputlog)
        app.alert("Exported")
    end
end

function init(plugin)
    Plugin = plugin

    plugin:newCommand{
        id = "ExportForPlaydate",
        title = "Export for Playdate...",
        group = "file_export_2",
        onclick = function()
            LoadLib("lib/exporter.lua")
            Execute()
        end,
        onenabled = function()
            return app.activeSprite ~= nil
        end
    }
end

--function exit(plugin)
--end
