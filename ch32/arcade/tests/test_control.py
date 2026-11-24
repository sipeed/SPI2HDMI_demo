"""Host controller selection must never open the unrelated NanoKVM UART."""
import importlib.util
from pathlib import Path
from types import SimpleNamespace as Port
path=Path(__file__).resolve().parents[4]/'tools/arcade_ch32.py'
spec=importlib.util.spec_from_file_location('control',path)
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
def p(name,vid,pid,serial='one'):
    return Port(device=name,vid=vid,pid=pid,serial_number=serial)
wch=p('/dev/ttyACM1',0x1a86,0x8010)
kvm=p('/dev/ttyUSB0',0x1a86,0x7523)
assert m.choose_port([kvm,wch])==wch.device
for ports,requested in [([kvm],None),([wch,kvm],kvm.device),([wch,wch],None)]:
    try:m.choose_port(ports,requested)
    except ValueError:pass
    else:raise AssertionError('Unsafe/ambiguous port accepted')
assert m.choose_port([wch,p('/dev/ttyACM2',0x1a86,0x8010,'two')],serial_number='two')=='/dev/ttyACM2'
# --any-port: explicit opt-in for a plain adapter, still refuses a bare/unknown path
ttl=p('/dev/ttyACM0',0x1a86,0x55d3)
assert m.choose_port([ttl,kvm],ttl.device,allow_any=True)==ttl.device
for requested in [None,'/dev/ttyACM9']:
    try:m.choose_port([ttl,kvm],requested,allow_any=True)
    except ValueError:pass
    else:raise AssertionError('--any-port accepted a missing port')
assert m.mask_packet(['a','j'])==b'\x91'
assert m.mask_packet(['j'])==b'\x81'
assert m.mask_packet([])==b'\x80'
print('PASS debugger identity selection; reject unrelated/ambiguous UARTs; key packets')
