module.exports = grammar({
  name: 'fuse',

  extras: $ => [ /\s/ ],

  word: $ => $.identifier,

  rules: {
    source_file: $ => seq(
      repeat($.binding),
      $.expression,
    ),

    binding: $ => seq(
      'let',
      field('name', $.identifier),
      '=',
      field('value', $.expression),
      ';',
    ),

    expression: $ => choice(
      $.pipeline,
      $.primary,
    ),

    pipeline: $ => prec.left(1, seq(
      field('input', $.primary),
      repeat1(seq(
        '.',
        field('stage', $.call),
      )),
    )),

    primary: $ => choice(
      $.call,
      $.identifier,
      $.number,
      seq('(', $.expression, ')'),
    ),

    call: $ => seq(
      field('function', $.identifier),
      '(',
      commaSep($.expression),
      ')',
    ),

    identifier: _ => /[A-Za-z_][A-Za-z0-9_]*/,

    number: _ => /-?[0-9]+(\.[0-9]+)?/,
  },
});

function commaSep(rule) {
  return optional(seq(rule, repeat(seq(',', rule))));
}
