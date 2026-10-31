if vim.treesitter and vim.treesitter.language then
  pcall(vim.treesitter.language.register, "fuse", { "fusegl", "fusetex", "fusemesh" })
end

vim.api.nvim_create_autocmd("FileType", {
  pattern = { "fusegl", "fusetex", "fusemesh" },
  callback = function(event)
    vim.bo[event.buf].commentstring = "// %s"
    vim.bo[event.buf].comments = "://"
    if vim.treesitter and vim.treesitter.start then
      pcall(vim.treesitter.start, event.buf, "fuse")
    end
  end,
})
