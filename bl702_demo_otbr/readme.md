
Bouffalo Lab Openthread Border Router runs all openthread stack and Border Router function on single chip - BL706, and uses Ethernet or Wi-Fi for backbone connection.

- Ethenet OTBR
- Wi-Fi OTBR, needs a BL602 module which runs a Wi-Fi transceiver. Please refer to `sdk_app_throughput` demo for more detail.

# Build

Type following command to build:

```shell
./genromap
```

- Build control on Ethernet OTBR or Wi-Fi OTBR
  - `CONFIG_USE_WIFI_BR=0` in `genromap` is for Ethernet OTBR build
  - `CONFIG_USE_WIFI_BR=1` in `genromap` is for Wi-Fi OTBR build
    - BL702 will toggle reset pin of BL602 during startup to make both chips have same fresh state. Please refer to `bl_factory_params_IoTKitA_32M_evb.dts` and configure `gpio_reset` with true settings.

- Build to auto form a Thread Network.
  - `CONFIG_THREAD_AUTO_START=1` in `genromap` enables to form a Thread network after it is assigned IPv6 address. 
    The network information as below macro defined in `include/main.h`. 
    ```c
    #define THREAD_CHANNEL      11
    #define THREAD_PANID        0xB702
    #define THREAD_EXTPANID     {0x11, 0x11, 0x11, 0x11, 0x22, 0x22, 0x22, 0x22}
    #define THREAD_NETWORK_KEY  {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff}
    ```
    
  - `CONFIG_THREAD_AUTO_START=0` in `genromap` doesn't form a Thread network. 
    Openthread command line is available on UART with `otc ` prefix to create and start a Thread network, such as `otc state`. 
  
- in `proj_config.mk`

  - `CONFIG_EASYFLASH_ENABLE`, save Thread network stack information to PSM partition managed by easyflash

  - `CONFIG_LITTLEFS`, save Thread network stack information to PSM partition managed by littlefs by default

    > littlefs requires more flash size and please refer to `bl702_demo_otbr/partition_cfg_2M.toml`

# Ethernet OTBR

  - Connect OTBR to IPv6 border router, and power up OTBR.
  - After IPv6 address is assigned, Thread network will automatically start if `CONFIG_THREAD_AUTO_START=1`.

# Wi-Fi OTBR

  - Development board hardware setup
    - Please refer to [BL702 SPI Wi-Fi guide](../sdk_app_throughput/bl702_master/ReadMe.md) for BL702 + BL602 development board setup.
    - Connect GPIO 11 of BL702 to **RESET PIN** of BL602 to do BL602 reset after BL706 startup.

  - Power up OTBR.
    - There may have some initial process block comnand link interactive during startup. Please try command when command line is available.

  - Open OTBR UART or USB CDC command line with serial port tool. And type following command to connect a AP.
    ```shell
    wifi_connect <wifi_ssid> <wifi_password>
    ```
    - When it connects AP succesfully, it will save SSID and password in flash and automatically connect this AP after power cycle.
    - Command `otc factory reset` is used to do factory reset.

  - After connected and IPv6 address assigned. The IP information will be printed out.

  - Then Thread network will automatically start if `CONFIG_THREAD_AUTO_START=1`. 

# Some helpful commands:
  - Command `ifconfig`: to get assigned IP address.
  - Command `otc state`: to get Thread state.
  - Command `otc br state`: to get Border Router state.
    - state `running` means border router is running after it attached and IP address is assigned.

# Thread 1.4 Credential Sharing POC

This firmware includes an experimental `tcs_pull` command that mirrors the host-side
`raw_dataset_pull` commissioner prototype. It uses a one-time credential/ePSKc to
open a DTLS EC-JPAKE session to another ecosystem's Border Agent, sends
`MGMT_ACTIVE_GET` to `/c/ag`, prints the returned raw Active Dataset TLVs, and can
optionally apply those TLVs to this BL702 OTBR.

```shell
tcs_pull <one_time_code> <border_agent_ip> <border_agent_port> [print|apply|apply-start]
tcs_scan [timeout_ms]
```

- `print` is the default and only prints `TCS_ACTIVE_DATASET_TLV=...`.
- `apply` validates the returned TLVs and calls `otDatasetSetActiveTlvs()`.
- `apply-start` disables Thread/IP6 first, sets the TLVs, then enables IP6 and Thread.
- `tcs_scan` uses one total timeout budget to find the first
  `_meshcop-e._udp.local` Border Agent endpoint. It does not use the one-time
  credential or pull credentials.
- `border_agent_ip` must currently be an IPv4/IPv6 literal. IPv6 link-local addresses
  may use `%<ifindex>`; without a scope, the infra netif index is used.
- The one-time credential is not printed back to the log. The log only prints
  `code_len`.

Expected high-level log sequence:

```text
TCS_PULL_BEGIN ...
TCS_PULL_STEP socket_connected
TCS_PULL_STEP dtls_handshake_start
TCS_PULL_STEP dtls_connected
TCS_PULL_STEP mgmt_active_get_sent ...
TCS_PULL_STEP mgmt_active_get_response ...
TCS_ACTIVE_DATASET_TLV=...
TCS_PULL_STEP dataset_parse status=OK
TCS_PULL_END status=ok
```

Expected scan log sequence:

```text
TCS_MDNS_SCAN_BEGIN service=_meshcop-e._udp.local ...
TCS_MDNS_RESULT index=0 instance=... host=... port=... network_name=... xpanid=...
TCS_MDNS_ENDPOINT index=0 addr_index=0 ip=... port=...
TCS_MDNS_TXT index=0 item=0 key=... value=... value_hex=...
TCS_MDNS_SCAN_END status=ok count=... elapsed_ms=...
```

Known limitations:

- `tcs_scan` only lists discovered Border Agents. `tcs_pull` still takes the selected
  Border Agent address and port as manual inputs.
- This is a take-credentials prototype. It does not advertise this OTBR's own
  Thread credentials to other ecosystems.
- End-to-end behavior still depends on the remote Border Agent accepting the supplied
  one-time credential for an EC-JPAKE secure session.
