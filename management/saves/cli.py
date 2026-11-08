"""Host administration for the mTCP save vault."""
import argparse
from pathlib import Path
from .vault import Vault
from .server import serve
from .backup import export, import_backup, verify
from .decisions import choose, fork
from .dashboard import report as dashboard_report, encode as dashboard_encode

from layout import VAULT
DEFAULT = VAULT

def parser(sub):
    p = sub.add_parser('saves', help='Manage the game-save vault')
    p.add_argument('--vault', type=Path, default=DEFAULT)
    commands = p.add_subparsers(dest='save_command', required=True)
    hub = commands.add_parser('serve')
    hub.add_argument('--bind', default='127.0.0.1')
    hub.add_argument('--port', type=int, default=8068)
    hub.add_argument('--token-file', type=Path, required=True)
    commands.add_parser('status')
    commands.add_parser('verify')
    dashboard=commands.add_parser('dashboard',help='Export a read-only LOC SHELL backup report')
    dashboard.add_argument('--output',type=Path,required=True)
    create = commands.add_parser('create')
    create.add_argument('game')
    create.add_argument('compatibility')
    create.add_argument('name')
    history = commands.add_parser('history')
    history.add_argument('campaign')
    for verb in ('resolve', 'restore', 'fork'):
        cmd = commands.add_parser(verb)
        cmd.add_argument('campaign')
        cmd.add_argument('snapshot')
        if verb == 'fork':
            cmd.add_argument('name')
        else:
            cmd.add_argument('--expected-head', required=True)
            cmd.add_argument('--operation', required=True)
    for verb in ('export', 'import'):
        cmd = commands.add_parser(verb)
        cmd.add_argument('path', type=Path)

def handle(a):
    verb = a.save_command
    if verb == 'dashboard':
        status=dashboard_report(a.vault)
        a.output.parent.mkdir(parents=True,exist_ok=True)
        a.output.write_bytes(dashboard_encode(status))
        return {'report':str(a.output),**status}
    if verb == 'serve':
        serve(a.vault, a.token_file, a.bind, a.port)
        return {'stopped': True}
    if verb == 'export':
        export(a.vault, a.path)
        return {'export': str(a.path)}
    if verb == 'import':
        import_backup(a.path, a.vault)
        return {'imported': str(a.vault)}
    if verb == 'verify':
        verify(a.vault)
        return {'verified': True}
    vault = Vault(a.vault)
    try:
        if verb == 'status':
            return {'campaigns': vault.campaigns()}
        if verb == 'create':
            return vault.create(a.game, a.compatibility, a.name)
        if verb in ('resolve', 'restore'):
            return {'head': choose(vault, a.campaign, a.snapshot, a.expected_head, a.operation)}
        if verb == 'fork':
            return fork(vault, a.campaign, a.snapshot, a.name)
        if verb == 'history':
            return vault.history(a.campaign)
    finally:
        vault.close()
