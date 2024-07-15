from __future__ import print_function
from __future__ import unicode_literals
import time
import re

from tiny_test_fw import DUT, App, TinyFW
from ttfw_bl import BL602App, BL602DUT


@TinyFW.test_method(app=BL602App.BL602App, dut=BL602DUT.BL602TyMbDUT, test_suite_name='sdk_app_heap_tc')
def sdk_app_heap_tc(env, extra_data):
    # first, flash dut
    # then, test
    dut = env.get_dut("port0", "fake app path")
    print('Flashing app')
    dut.flash_app(env.log_path, env.get_variable('flash'))
    print('Starting app')
    dut.start_app()

    try:
        time.sleep(3)

        dut.write('heap_test')
        dut.expect('mem heap test ok!', timeout=5)
        dut.halt()

    except DUT.ExpectTimeout:
        print('ENV_TEST_FAILURE: BL602 example test failed')
        raise


if __name__ == '__main__':
    sdk_app_heap_tc()
