"""Explicit, archived conflict decisions. No timestamp-based winners."""
import secrets

def choose(vault, campaign, selected, expected, operation):
    """Publish a preserved version as a new revision; never rewrite history."""
    source = vault.snapshot(selected)
    if source['campaign'] != campaign:
        raise ValueError('Selected snapshot belongs to another campaign')
    prior = vault.db.execute('SELECT * FROM publications WHERE operation=?', (operation,)).fetchone()
    if prior:
        previous = vault.snapshot(prior['snapshot'])
        if (prior['campaign'] != campaign or prior['expected'] != expected or
                previous['files'] != source['files'] or selected not in previous['parents']):
            raise ValueError('Operation ID reused with a different decision')
        return prior['snapshot']
    parents = list(dict.fromkeys(p for p in (expected, selected) if p))
    # Capture first is harmless if CAS fails: both alternatives stay in history.
    snapshot = vault.capture(campaign, source['files'], parents, 'host-decision', 'dosctl')
    return vault.publish(campaign, snapshot, expected, operation)

def fork(vault, campaign, selected, name):
    source = vault.snapshot(selected)
    original = vault.campaign(campaign)
    if source['campaign'] != campaign:
        raise ValueError('Selected snapshot belongs to another campaign')
    target = vault.create(original['game'], original['compatibility'], name)
    snapshot = vault.capture(target['id'], source['files'], [], 'host-fork', 'dosctl')
    vault.publish(target['id'], snapshot, None, secrets.token_hex(16))
    return vault.campaign(target['id'])
