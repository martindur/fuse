local M = {}

function M.setup(options)
  options = options or {}
  local command = options.command or { "grafic", "lsp" }
  local group = vim.api.nvim_create_augroup("grafic_lsp", { clear = true })

  vim.api.nvim_create_autocmd("FileType", {
    group = group,
    pattern = { "graficgl", "grafictex", "graficmesh" },
    callback = function(event)
      local filename = vim.api.nvim_buf_get_name(event.buf)
      local directory = filename == "" and vim.fn.getcwd() or vim.fs.dirname(filename)
      local marker = vim.fs.find(".git", { upward = true, path = directory })[1]
      vim.lsp.start({
        name = "grafic",
        cmd = command,
        root_dir = marker and vim.fs.dirname(marker) or directory,
      }, { bufnr = event.buf })
    end,
  })
end

return M
