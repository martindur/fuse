module.exports = grammar({
  name: 'grafic',

  extras: $ => [ /\s/, $.comment ],

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
      $.field_access,
      $.primary,
    ),

    pipeline: $ => prec.left(1, seq(
      field('input', $.expression),
      '.',
      field('stage', $.call),
    )),

    field_access: $ => prec.left(1, seq(
      field('input', $.expression),
      '.',
      field('field', $.identifier),
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

    comment: _ => token(seq('//', /[^\r\n]*/)),
  },
});

function commaSep(rule) {
  return optional(seq(rule, repeat(seq(',', rule))));
}
