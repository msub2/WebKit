/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Tests submit validation.

Note: destroyed buffer/texture/querySet are tested in destroyed/. (unless it gets moved here)
Note: buffer map state is tested in ./buffer_mapped.spec.ts.
`;
import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { ValidationTest } from '../validation_test.js';

export const g = makeTestGroup(ValidationTest);

g.test('command_buffer,device_mismatch')
  .desc(
    `
    Tests submit cannot be called with command buffers created from another device
    Test with two command buffers to make sure all command buffers can be validated:
    - cb0 and cb1 from same device
    - cb0 and cb1 from different device
    `
  )
  .paramsSubcasesOnly([
    { cb0Mismatched: false, cb1Mismatched: false }, // control case
    { cb0Mismatched: true, cb1Mismatched: false },
    { cb0Mismatched: false, cb1Mismatched: true },
  ])
  .unimplemented();
