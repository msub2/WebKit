/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ import { assert, ErrorWithExtra, iterRange, range } from '../../common/util/util.js';

import { float16BitsToFloat32 } from './conversion.js';
import { generatePrettyTable } from './pretty_diff_tables.js';

/** Generate an expected value at `index`, to test for equality with the actual value. */

/**
 * Check whether two `TypedArray`s have equal contents.
 * Returns `undefined` if the check passes, or an `Error` if not.
 */
export function checkElementsEqual(actual, expected) {
  assert(actual.constructor === expected.constructor, 'TypedArray type mismatch');
  assert(actual.length === expected.length, 'size mismatch');
  return checkElementsEqualGenerated(actual, i => expected[i]);
}

/**
 * Check whether each value in a `TypedArray` is between the two corresponding "expected" values
 * (either `a(i) <= actual[i] <= b(i)` or `a(i) >= actual[i] => b(i)`).
 */
export function checkElementsBetween(actual, expected) {
  const error = checkElementsPassPredicate(
    actual,
    (index, value) =>
      value >= Math.min(expected[0](index), expected[1](index)) &&
      value <= Math.max(expected[0](index), expected[1](index)),
    {
      predicatePrinter: [
        { leftHeader: 'between', getValueForCell: index => expected[0](index) },
        { leftHeader: 'and', getValueForCell: index => expected[1](index) },
      ],
    }
  );

  // If there was an error, extend it with additional extras.
  return error ? new ErrorWithExtra(error, () => ({ expected })) : undefined;
}

/**
 * Equivalent to {@link checkElementsBetween} but interpret values as float16 and convert to JS number before comparison.
 */
export function checkElementsFloat16Between(actual, expected) {
  assert(actual.BYTES_PER_ELEMENT === 2, 'bytes per element need to be 2 (16bit)');
  const actualF32 = new Float32Array(actual.length);
  actual.forEach((v, i) => {
    actualF32[i] = float16BitsToFloat32(v);
  });
  const expectedF32 = [new Float32Array(expected[0].length), new Float32Array(expected[1].length)];
  expected[0].forEach((v, i) => {
    expectedF32[0][i] = float16BitsToFloat32(v);
  });
  expected[1].forEach((v, i) => {
    expectedF32[1][i] = float16BitsToFloat32(v);
  });

  const error = checkElementsPassPredicate(
    actualF32,
    (index, value) =>
      value >= Math.min(expectedF32[0][index], expectedF32[1][index]) &&
      value <= Math.max(expectedF32[0][index], expectedF32[1][index]),
    {
      predicatePrinter: [
        { leftHeader: 'between', getValueForCell: index => expectedF32[0][index] },
        { leftHeader: 'and', getValueForCell: index => expectedF32[1][index] },
      ],
    }
  );

  // If there was an error, extend it with additional extras.
  return error ? new ErrorWithExtra(error, () => ({ expectedF32 })) : undefined;
}

/**
 * Check whether each value in a `TypedArray` is equal to one of the two corresponding "expected"
 * values (either `actual[i] === a[i]` or `actual[i] === b[i]`)
 */
export function checkElementsEqualEither(actual, expected) {
  const error = checkElementsPassPredicate(
    actual,
    (index, value) => value === expected[0][index] || value === expected[1][index],
    {
      predicatePrinter: [
        { leftHeader: 'either', getValueForCell: index => expected[0][index] },
        { leftHeader: 'or', getValueForCell: index => expected[1][index] },
      ],
    }
  );

  // If there was an error, extend it with additional extras.
  return error ? new ErrorWithExtra(error, () => ({ expected })) : undefined;
}

/**
 * Check whether a `TypedArray`'s contents equal the values produced by a generator function.
 * Returns `undefined` if the check passes, or an `Error` if not.
 *
 * ```text
 * Array had unexpected contents at indices 2 through 19.
 *  Starting at index 1:
 *    actual == 0x: 00 fe ff 00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f 00
 *    failed ->        xx xx    xx xx xx xx xx xx xx xx xx xx xx xx xx xx xx
 *  expected ==     00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
 * ```
 *
 * ```text
 * Array had unexpected contents at indices 2 through 29.
 *  Starting at index 1:
 *    actual ==  0.000 -2.000e+100 -1.000e+100 0.000 1.000e+100 2.000e+100 3.000e+100 4.000e+100 5.000e+100 6.000e+100 7.000e+100 ...
 *    failed ->                 xx          xx               xx         xx         xx         xx         xx         xx         xx ...
 *  expected ==  0.000       0.000       0.000 0.000      0.000      0.000      0.000      0.000      0.000      0.000      0.000 ...
 * ```
 */
export function checkElementsEqualGenerated(actual, generator) {
  const error = checkElementsPassPredicate(actual, (index, value) => value === generator(index), {
    predicatePrinter: [{ leftHeader: 'expected ==', getValueForCell: index => generator(index) }],
  });

  // If there was an error, extend it with additional extras.
  return error ? new ErrorWithExtra(error, () => ({ generator })) : undefined;
}

/**
 * Check whether a `TypedArray`'s values pass the provided predicate function.
 * Returns `undefined` if the check passes, or an `Error` if not.
 */
export function checkElementsPassPredicate(actual, predicate, { predicatePrinter }) {
  const size = actual.length;
  const ctor = actual.constructor;
  const printAsFloat = ctor === Float32Array || ctor === Float64Array;

  let failedElementsFirstMaybe = undefined;
  /** Sparse array with `true` for elements that failed. */
  const failedElements = [];
  for (let i = 0; i < size; ++i) {
    if (!predicate(i, actual[i])) {
      failedElementsFirstMaybe ??= i;
      failedElements[i] = true;
    }
  }

  if (failedElementsFirstMaybe === undefined) {
    return undefined;
  }
  const failedElementsFirst = failedElementsFirstMaybe;
  const failedElementsLast = failedElements.length - 1;

  // Include one extra non-failed element at the beginning and end (if they exist), for context.
  const printElementsStart = Math.max(0, failedElementsFirst - 1);
  const printElementsEnd = Math.min(size, failedElementsLast + 2);
  const printElementsCount = printElementsEnd - printElementsStart;

  const numberToString = printAsFloat
    ? n => n.toPrecision(4)
    : n => intToPaddedHex(n, { byteLength: ctor.BYTES_PER_ELEMENT });
  const numberPrefix = printAsFloat ? '' : '0x:';

  const printActual = actual.subarray(printElementsStart, printElementsEnd);
  const printExpected = [];
  if (predicatePrinter) {
    for (const { leftHeader, getValueForCell: cell } of predicatePrinter) {
      printExpected.push(
        (function* () {
          yield* [leftHeader, ''];
          yield* iterRange(printElementsCount, i => cell(printElementsStart + i));
        })()
      );
    }
  }

  const printFailedValueMarkers = (function* () {
    yield* ['failed ->', ''];
    yield* range(printElementsCount, i => (failedElements[printElementsStart + i] ? 'xx' : ''));
  })();

  const opts = {
    fillToWidth: 120,
    numberToString,
  };

  const msg = `Array had unexpected contents at indices ${failedElementsFirst} through ${failedElementsLast}.
 Starting at index ${printElementsStart}:
${generatePrettyTable(opts, [
  ['actual ==', numberPrefix, ...printActual],
  printFailedValueMarkers,
  ...printExpected,
])}`;
  return new ErrorWithExtra(msg, () => ({
    actual: actual.slice(),
  }));
}

// Helper helpers

/** Convert an integral `number` into a hex string, padded to the specified `byteLength`. */
function intToPaddedHex(number, { byteLength }) {
  assert(Number.isInteger(number), 'number must be integer');
  let s = Math.abs(number).toString(16);
  if (byteLength) s = s.padStart(byteLength * 2, '0');
  if (number < 0) s = '-' + s;
  return s;
}
