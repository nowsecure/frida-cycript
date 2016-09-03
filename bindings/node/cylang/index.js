'use strict';

const binding = require('bindings')('cylang_binding');

module.exports = {
  compile: compile
};

function compile(source, options = {}) {
  const {strict = false, pretty = false} = options;

  return binding.compile(source, strict, pretty);
}
