if vim.treesitter and vim.treesitter.language then
  pcall(vim.treesitter.language.register, "fuse", { "fusegl", "fusetex", "fusemesh" })
end

if vim.treesitter and vim.treesitter.start then
  vim.api.nvim_create_autocmd("FileType", {
    pattern = { "fusegl", "fusetex", "fusemesh" },
    callback = function(event)
      pcall(vim.treesitter.start, event.buf, "fuse")
    end,
  })
end
