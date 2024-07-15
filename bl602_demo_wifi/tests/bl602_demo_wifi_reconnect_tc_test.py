from __future__ import print_function
from __future__ import unicode_literals
import time
import re
import os

from tiny_test_fw import DUT, App, TinyFW
from ttfw_bl import BL602App, BL602DUT


@TinyFW.test_method(app=BL602App.BL602App, dut=BL602DUT.BL602TyMbDUT, test_suite_name='bl602_demo_wifi_reconnect_tc')
def bl602_demo_wifi_reconnect_tc(env, extra_data):
    # first, flash dut
    # then, test
    dut = env.get_dut("port0", "fake app path")
    print('Flashing app')
    dut.flash_app(env.log_path, env.get_variable('flash'))
    print('Starting app')
    dut.start_app()

    try:
        time.sleep(3)
        for i in range(5):
            #wifi_sta_connect
            bssid = os.getenv('TEST_ROUTER_SSID')
            pwd = os.getenv('TEST_ROUTER_PASSWORD')
            cmd = ("wifi_sta_connect", bssid, pwd)
            cmd_wifi_connect = ' '.join(cmd)
            dut.write(cmd_wifi_connect)
            dut.expect("Entering wifiConnected_IPOK state", timeout=50)
            time.sleep(20)
            dut.write('wifi_state')
            dut.expect("wifi state connected ip got", timeout=1)
            time.sleep(3)
            dut.write('wifi_sta_disconnect')
            time.sleep(5)
        dut.halt()
    except DUT.ExpectTimeout:
        dut.write('p 0')
        result_text = dut.read()
        print(result_text)
        print('ENV_TEST_FAILURE: BL602 wifi test failed')
        raise


if __name__ == '__main__':
    bl602_demo_wifi_reconnect_tc()
