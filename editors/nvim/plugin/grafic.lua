if vim.treesitter and vim.treesitter.language then
  pcall(vim.treesitter.language.register, "grafic", { "graficgl", "grafictex", "graficmesh" })
end

vim.api.nvim_set_hl(0, "@function.builtin.grafic", { default = true, link = "Keyword" })

vim.api.nvim_create_autocmd("FileType", {
  pattern = { "graficgl", "grafictex", "graficmesh" },
  callback = function(event)
    vim.bo[event.buf].commentstring = "// %s"
    vim.bo[event.buf].comments = "://"
    if vim.treesitter and vim.treesitter.start then
      pcall(vim.treesitter.start, event.buf, "grafic")
    end
  end,
})
