/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
API validation tests for clearBuffer.
`;
import { makeTestGroup } from '../../../../../common/framework/test_group.js';
import { kBufferUsages } from '../../../../capability_info.js';
import { kMaxSafeMultipleOf8 } from '../../../../util/math.js';
import { ValidationTest } from '../../validation_test.js';

class F extends ValidationTest {
  TestClearBuffer(options) {
    const { buffer, offset, size, isSuccess } = options;

    const commandEncoder = this.device.createCommandEncoder();
    commandEncoder.clearBuffer(buffer, offset, size);

    this.expectValidationError(() => {
      commandEncoder.finish();
    }, !isSuccess);
  }
}

export const g = makeTestGroup(F);

g.test('invalid_buffer')
  .desc(`Test that clearing an error buffer fails.`)
  .fn(async t => {
    const errorBuffer = t.getErrorBuffer();

    t.TestClearBuffer({
      buffer: errorBuffer,
      offset: 0,
      size: 8,
      isSuccess: false,
    });
  });

g.test('default_args')
  .desc(`Test that calling clearBuffer with a default offset and size is valid.`)
  .paramsSubcasesOnly([
    { offset: undefined, size: undefined },
    { offset: 4, size: undefined },
    { offset: undefined, size: 8 },
  ])
  .fn(async t => {
    const { offset, size } = t.params;

    const buffer = t.device.createBuffer({
      size: 16,
      usage: GPUBufferUsage.COPY_DST,
    });

    t.TestClearBuffer({
      buffer,
      offset,
      size,
      isSuccess: true,
    });
  });

g.test('buffer_usage')
  .desc(`Test that only buffers with COPY_DST usage are valid to use with copyBuffers.`)
  .paramsSubcasesOnly(u =>
    u //
      .combine('usage', kBufferUsages)
  )
  .fn(async t => {
    const { usage } = t.params;

    const buffer = t.device.createBuffer({
      size: 16,
      usage,
    });

    t.TestClearBuffer({
      buffer,
      offset: 0,
      size: 16,
      isSuccess: usage === GPUBufferUsage.COPY_DST,
    });
  });

g.test('size_alignment')
  .desc(
    `
    Test that the clear size must be 4 byte aligned.
    - Test size is not a multiple of 4.
    - Test size is 0.
    - Test size overflows the buffer size.
    - Test size is omitted.
  `
  )
  .paramsSubcasesOnly([
    { size: 0, _isSuccess: true },
    { size: 2, _isSuccess: false },
    { size: 4, _isSuccess: true },
    { size: 5, _isSuccess: false },
    { size: 8, _isSuccess: true },
    { size: 20, _isSuccess: false },
    { size: undefined, _isSuccess: true },
  ])
  .fn(async t => {
    const { size, _isSuccess: isSuccess } = t.params;

    const buffer = t.device.createBuffer({
      size: 16,
      usage: GPUBufferUsage.COPY_DST,
    });

    t.TestClearBuffer({
      buffer,
      offset: 0,
      size,
      isSuccess,
    });
  });

g.test('offset_alignment')
  .desc(
    `
    Test that the clear offsets must be 4 byte aligned.
    - Test offset is not a multiple of 4.
    - Test offset is larger than the buffer size.
    - Test offset is omitted.
  `
  )
  .paramsSubcasesOnly([
    { offset: 0, _isSuccess: true },
    { offset: 2, _isSuccess: false },
    { offset: 4, _isSuccess: true },
    { offset: 5, _isSuccess: false },
    { offset: 8, _isSuccess: true },
    { offset: 20, _isSuccess: false },
    { offset: undefined, _isSuccess: true },
  ])
  .fn(async t => {
    const { offset, _isSuccess: isSuccess } = t.params;

    const buffer = t.device.createBuffer({
      size: 16,
      usage: GPUBufferUsage.COPY_DST,
    });

    t.TestClearBuffer({
      buffer,
      offset,
      size: 8,
      isSuccess,
    });
  });

g.test('overflow')
  .desc(`Test that clears which may cause arthimetic overflows are invalid.`)
  .paramsSubcasesOnly([
    { offset: 0, size: kMaxSafeMultipleOf8 },
    { offset: 16, size: kMaxSafeMultipleOf8 },
    { offset: kMaxSafeMultipleOf8, size: 16 },
    { offset: kMaxSafeMultipleOf8, size: kMaxSafeMultipleOf8 },
  ])
  .fn(async t => {
    const { offset, size } = t.params;

    const buffer = t.device.createBuffer({
      size: 16,
      usage: GPUBufferUsage.COPY_DST,
    });

    t.TestClearBuffer({
      buffer,
      offset,
      size,
      isSuccess: false,
    });
  });

g.test('out_of_bounds')
  .desc(`Test that clears which exceed the buffer bounds are invalid.`)
  .paramsSubcasesOnly([
    { offset: 0, size: 32, _isSuccess: true },
    { offset: 0, size: 36 },
    { offset: 32, size: 0, _isSuccess: true },
    { offset: 32, size: 4 },
    { offset: 36, size: 4 },
    { offset: 36, size: 0 },
    { offset: 20, size: 16 },
    { offset: 20, size: 12, _isSuccess: true },
  ])
  .fn(async t => {
    const { offset, size, _isSuccess = false } = t.params;

    const buffer = t.device.createBuffer({
      size: 32,
      usage: GPUBufferUsage.COPY_DST,
    });

    t.TestClearBuffer({
      buffer,
      offset,
      size,
      isSuccess: _isSuccess,
    });
  });
