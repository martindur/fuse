local M = {}

function M.setup(options)
  options = options or {}
  local commands = vim.tbl_extend("force", {
    fusegl = { "fusegl", "lsp" },
    fusetex = { "fusetex", "lsp" },
    fusemesh = { "fusemesh", "lsp" },
  }, options.commands or {})
  local group = vim.api.nvim_create_augroup("fuse_lsp", { clear = true })

  vim.api.nvim_create_autocmd("FileType", {
    group = group,
    pattern = { "fusegl", "fusetex", "fusemesh" },
    callback = function(event)
      local filename = vim.api.nvim_buf_get_name(event.buf)
      local directory = filename == "" and vim.fn.getcwd() or vim.fs.dirname(filename)
      local marker = vim.fs.find(".git", { upward = true, path = directory })[1]
      local filetype = vim.bo[event.buf].filetype
      vim.lsp.start({
        name = filetype,
        cmd = commands[filetype],
        root_dir = marker and vim.fs.dirname(marker) or directory,
        bufnr = event.buf,
      })
    end,
  })
end

return M
