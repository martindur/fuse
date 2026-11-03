"let" @keyword
"=" @operator

"(" @punctuation.bracket
")" @punctuation.bracket
"," @punctuation.delimiter
";" @punctuation.delimiter

(call function: (identifier) @function.call)
(binding name: (identifier) @variable)
(identifier) @variable
(number) @number
(comment) @comment

((call function: (identifier) @function.builtin.grafic)
  (#eq? @function.builtin.grafic "show")
  (#set! priority 110))
