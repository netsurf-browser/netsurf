#!/usr/bin/python3
#
# Copyright 2019 Daniel Silverstone <dsilvers@digital-scurf.org>
#
# This file is part of NetSurf, http://www.netsurf-browser.org/
#
# NetSurf is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
#
# NetSurf is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

"""
runs tests in monkey as defined in a yaml file
"""

# pylint: disable=locally-disabled, missing-docstring

import sys
import getopt
import time
import yaml

from monkeyfarmer import Browser


class DriverBrowser(Browser):
    def __init__(self, *args, **kwargs):
        super(DriverBrowser, self).__init__(*args, **kwargs)
        self.auth = []
        self.cert = []

    def add_auth(self, url, realm, username, password):
        self.auth.append((url, realm, username, password))

    def remove_auth(self, url, realm, username, password):
        keep = []

        def matches(first, second):
            if first is None or second is None:
                return True
            return first == second

        for (iurl, irealm, iusername, ipassword) in self.auth:
            if not (matches(url, iurl) or
                    matches(realm, irealm) or
                    matches(username, iusername) or
                    matches(password, ipassword)):
                keep.append((iurl, irealm, iusername, ipassword))
        self.auth = keep

    def handle_ready_login(self, logwin):
        # We have logwin.{url,username,password,realm}
        # We must logwin.send_{username,password}(xxx)
        # We may logwin.go()
        # We may logwin.destroy()
        def matches(first, second):
            if first is None or second is None:
                return True
            return first == second
        candidates = []
        for (url, realm, username, password) in self.auth:
            score = 0
            if matches(url, logwin.url):
                score += 1
            if matches(realm, logwin.realm):
                score += 1
            if matches(username, logwin.username):
                score += 1
            if score > 0:
                candidates.append((score, username, password))
        if candidates:
            candidates.sort()
            (score, username, password) = candidates[-1]
            print("401: Found candidate {}/{} with score {}".format(username, password, score))
            logwin.send_username(username)
            logwin.send_password(password)
            logwin.go()
        else:
            print("401: No candidate found, cancelling login box")
            logwin.destroy()

    def add_cert(self, url):
        # add sll certificate error exception
        self.cert.append(url)

    def remove_cert(self, url):
        keep = []

        def matches(first, second):
            if first is None or second is None:
                return True
            return first == second

        for iurl in self.cert:
            if not matches(url, iurl):
                keep.append(iurl)
        self.cert = keep

    def handle_ready_sslcert(self, cwin):

        def matches(first, second):
            if first is None or second is None:
                return True
            return first == second

        candidates = []
        for url in self.cert:
            score = 0
            if matches(url, cwin.url):
                score += 1
            if score > 0:
                candidates.append((score, url))
        if candidates:
            candidates.sort()
            (score, url) = candidates[-1]
            print("SSLCert: Found candidate {} with score {}".format(url, score))
            cwin.go()
        else:
            print("SSLCert: No candidate found, cancelling sslcert box")
            cwin.destroy()


def print_usage():
    print('Usage:')
    print('  ' + sys.argv[0] + ' -m <path to monkey> -t <path to test> [-w <wrapper arguments>]')


def parse_argv(argv):

    # pylint: disable=locally-disabled, unused-variable

    path_monkey = ''
    path_test = ''
    wrapper = None
    try:
        opts, args = getopt.getopt(argv, "hm:t:w:", ["monkey=", "test=", "wrapper="])
    except getopt.GetoptError:
        print_usage()
        sys.exit(2)
    for opt, arg in opts:
        if opt == '-h':
            print_usage()
            sys.exit()
        elif opt in ("-m", "--monkey"):
            path_monkey = arg
        elif opt in ("-t", "--test"):
            path_test = arg
        elif opt in ("-w", "--wrapper"):
            if wrapper is None:
                wrapper = []
            wrapper.extend(arg.split())

    if path_monkey == '':
        print_usage()
        sys.exit()
    if path_test == '':
        print_usage()
        sys.exit()

    return path_monkey, path_test, wrapper


def load_test_plan(path):

    # pylint: disable=locally-disabled, broad-except

    plan = []
    with open(path, 'r') as stream:
        try:
            plan = (yaml.load(stream))
        except Exception as exc:
            print(exc)
    return plan


def get_indent(ctx):
    return '  ' * ctx["depth"]


def print_test_plan_info(ctx, plan):

    # pylint: disable=locally-disabled, unused-argument

    print('Running test: [' + plan["group"] + '] ' + plan["title"])


def assert_browser(ctx):
    assert ctx['browser'].started
    assert not ctx['browser'].stopped


def conds_met(ctx, conds):
    # for each condition listed determine if they have been met
    #  efectively this is condition1 | condition2
    for cond in conds:
        if 'timer' in cond.keys():
            timer = cond['timer']
            elapsed = cond['elapsed']
            assert_browser(ctx)
            assert ctx['timers'].get(timer) is not None
            taken = time.time() - ctx['timers'][timer]["start"]
            if taken >= elapsed:
                return True
        elif 'window' in cond.keys():
            status = cond['status']
            window = cond['window']
            assert status == "complete"  # TODO: Add more status support?
            if window == "*all*":
                # all windows must be not throbbing
                throbbing = False
                for win in ctx['windows'].items():
                    if win[1].throbbing:
                        throbbing = True
                if not throbbing:
                    return True
            else:
                win = ctx['windows'][window]
                if win.throbbing is False:
                    return True
        else:
            raise AssertionError("Unknown condition: {}".format(repr(cond)))

    return False


def run_test_step_action_launch(ctx, step):
    print(get_indent(ctx) + "Action: " + step["action"])
    assert ctx.get('browser') is None
    assert ctx.get('windows') is None
    ctx['browser'] = DriverBrowser(
        monkey_cmd=[ctx["monkey"]],
        quiet=True,
        wrapper=ctx.get("wrapper"))
    assert_browser(ctx)
    ctx['windows'] = dict()
    for option in step.get('options', []):
        print(get_indent(ctx) + "        " + option)
        ctx['browser'].pass_options(option)


def run_test_step_action_window_new(ctx, step):

    # pylint: disable=locally-disabled, invalid-name

    print(get_indent(ctx) + "Action: " + step["action"])
    tag = step['tag']
    assert_browser(ctx)
    assert ctx['windows'].get(tag) is None
    ctx['windows'][tag] = ctx['browser'].new_window(url=step.get('url'))


def run_test_step_action_window_close(ctx, step):

    # pylint: disable=locally-disabled, invalid-name

    print(get_indent(ctx) + "Action: " + step["action"])
    assert_browser(ctx)
    tag = step['window']
    assert ctx['windows'].get(tag) is not None
    win = ctx['windows'].pop(tag)
    win.kill()
    win.wait_until_dead()
    assert not win.alive


def run_test_step_action_navigate(ctx, step):
    print(get_indent(ctx) + "Action: " + step["action"])
    assert_browser(ctx)
    if 'url' in step.keys():
        url = step['url']
    elif 'repeaturl' in step.keys():
        repeat = ctx['repeats'].get(step['repeaturl'])
        assert repeat is not None
        assert repeat.get('values') is not None
        url = repeat['values'][repeat['i']]
    else:
        url = None
    assert url is not None
    tag = step['window']
    print(get_indent(ctx) + "        " + tag + " --> " + url)
    win = ctx['windows'].get(tag)
    assert win is not None
    win.go(url)


def run_test_step_action_stop(ctx, step):
    print(get_indent(ctx) + "Action: " + step["action"])
    assert_browser(ctx)
    tag = step['window']
    win = ctx['windows'].get(tag)
    assert win is not None
    win.stop()


def run_test_step_action_sleep_ms(ctx, step):
    print(get_indent(ctx) + "Action: " + step["action"])
    conds = step['conditions']
    sleep_time = step['time']
    sleep = 0
    have_repeat = False
    if isinstance(sleep_time, str):
        assert ctx['repeats'].get(sleep_time) is not None
        repeat = ctx['repeats'].get(sleep_time)
        sleep = repeat["i"] / 1000
        start = repeat["start"]
        have_repeat = True
    else:
        sleep = time / 1000
        start = time.time()

    while True:
        slept = time.time() - start
        if conds_met(ctx, conds):
            if have_repeat:
                ctx['repeats'][sleep_time]["loop"] = False
            print(get_indent(ctx) + "        Condition met after {}s".format(slept))
            break
        elif slept > sleep:
            print(get_indent(ctx) + "        Condition not met after {}s".format(sleep))
            break
        else:
            ctx['browser'].farmer.loop(once=True)


def run_test_step_action_block(ctx, step):
    print(get_indent(ctx) + "Action: " + step["action"])
    conds = step['conditions']
    assert_browser(ctx)

    while not conds_met(ctx, conds):
        ctx['browser'].farmer.loop(once=True)


def run_test_step_action_repeat(ctx, step):
    print(get_indent(ctx) + "Action: " + step["action"])
    tag = step['tag']
    assert ctx['repeats'].get(tag) is None
    ctx['repeats'][tag] = {"loop": True, }

    if 'min' in step.keys():
        ctx['repeats'][tag]["i"] = step["min"]
    else:
        ctx['repeats'][tag]["i"] = 0

    if 'step' in step.keys():
        ctx['repeats'][tag]["step"] = step["step"]
    else:
        ctx['repeats'][tag]["step"] = 1

    if 'values' in step.keys():
        ctx['repeats'][tag]['values'] = step["values"]
    else:
        ctx['repeats'][tag]['values'] = None

    while ctx['repeats'][tag]["loop"]:
        ctx['repeats'][tag]["start"] = time.time()
        ctx["depth"] += 1
        for stp in step["steps"]:
            run_test_step(ctx, stp)
        ctx['repeats'][tag]["i"] += ctx['repeats'][tag]["step"]
        if ctx['repeats'][tag]['values'] is not None:
            if ctx['repeats'][tag]["i"] >= len(ctx['repeats'][tag]['values']):
                ctx['repeats'][tag]["loop"] = False
        ctx["depth"] -= 1


def run_test_step_action_plot_check(ctx, step):
    print(get_indent(ctx) + "Action: " + step["action"])
    assert_browser(ctx)
    win = ctx['windows'][step['window']]
    if 'checks' in step.keys():
        checks = step['checks']
    else:
        checks = {}
    all_text_list = []
    bitmaps = []
    for plot in win.redraw():
        if plot[0] == 'TEXT':
            all_text_list.extend(plot[6:])
        if plot[0] == 'BITMAP':
            bitmaps.append(plot[1:])
    all_text = " ".join(all_text_list)
    for check in checks:
        if 'text-contains' in check.keys():
            print("Check {} in {}".format(repr(check['text-contains']), repr(all_text)))
            assert check['text-contains'] in all_text
        elif 'text-not-contains' in check.keys():
            print("Check {} NOT in {}".format(repr(check['text-not-contains']), repr(all_text)))
            assert check['text-not-contains'] not in all_text
        elif 'bitmap-count' in check.keys():
            print("Check bitmap count is {}".format(int(check['bitmap-count'])))
            assert len(bitmaps) == int(check['bitmap-count'])
        else:
            raise AssertionError("Unknown check: {}".format(repr(check)))


def run_test_step_action_timer_start(ctx, step):

    # pylint: disable=locally-disabled, invalid-name

    print(get_indent(ctx) + "Action: " + step["action"])
    tag = step['timer']
    assert_browser(ctx)
    assert ctx['timers'].get(tag) is None
    ctx['timers'][tag] = {}
    ctx['timers'][tag]["start"] = time.time()


def run_test_step_action_timer_restart(ctx, step):

    # pylint: disable=locally-disabled, invalid-name

    print(get_indent(ctx) + "Action: " + step["action"])
    timer = step['timer']
    assert_browser(ctx)
    assert ctx['timers'].get(timer) is not None
    taken = time.time() - ctx['timers'][timer]["start"]
    print("{}        {} restarted at: {:.2f}s".format(get_indent(ctx), timer, taken))
    ctx['timers'][timer]["taken"] = taken
    ctx['timers'][timer]["start"] = time.time()


def run_test_step_action_timer_stop(ctx, step):
    print(get_indent(ctx) + "Action: " + step["action"])
    timer = step['timer']
    assert_browser(ctx)
    assert ctx['timers'].get(timer) is not None
    taken = time.time() - ctx['timers'][timer]["start"]
    print("{}        {} took: {:.2f}s".format(get_indent(ctx), timer, taken))
    ctx['timers'][timer]["taken"] = taken


def run_test_step_action_timer_check(ctx, step):

    # pylint: disable=locally-disabled, invalid-name

    print(get_indent(ctx) + "Action: " + step["action"])
    condition = step["condition"].split()
    assert len(condition) == 3
    timer1 = ctx['timers'].get(condition[0])
    timer2 = ctx['timers'].get(condition[2])
    assert timer1 is not None
    assert timer2 is not None
    assert timer1["taken"] is not None
    assert timer2["taken"] is not None
    assert condition[1] in ('<', '>')
    if condition[1] == '<':
        assert timer1["taken"] < timer2["taken"]
    elif condition[1] == '>':
        assert timer1["taken"] > timer2["taken"]


def run_test_step_action_add_auth(ctx, step):
    print(get_indent(ctx) + "Action:" + step["action"])
    assert_browser(ctx)
    browser = ctx['browser']
    browser.add_auth(step.get("url"), step.get("realm"),
                     step.get("username"), step.get("password"))


def run_test_step_action_remove_auth(ctx, step):

    # pylint: disable=locally-disabled, invalid-name

    print(get_indent(ctx) + "Action:" + step["action"])
    assert_browser(ctx)
    browser = ctx['browser']
    browser.remove_auth(step.get("url"), step.get("realm"),
                        step.get("username"), step.get("password"))


def run_test_step_action_add_cert(ctx, step):
    print(get_indent(ctx) + "Action:" + step["action"])
    assert_browser(ctx)
    browser = ctx['browser']
    browser.add_cert(step.get("url"))


def run_test_step_action_remove_cert(ctx, step):

    # pylint: disable=locally-disabled, invalid-name

    print(get_indent(ctx) + "Action:" + step["action"])
    assert_browser(ctx)
    browser = ctx['browser']
    browser.remove_cert(step.get("url"))


def run_test_step_action_clear_log(ctx, step):
    print(get_indent(ctx) + "Action: " + step["action"])
    assert_browser(ctx)
    tag = step['window']
    print(get_indent(ctx) + "        " + tag + " Log cleared")
    win = ctx['windows'].get(tag)
    assert win is not None
    win.clear_log()


def run_test_step_action_wait_log(ctx, step):
    print(get_indent(ctx) + "Action: " + step["action"])
    assert_browser(ctx)
    tag = step['window']
    source = step.get('source')
    foldable = step.get('foldable')
    level = step.get('level')
    substr = step.get('substring')
    print(get_indent(ctx) + "        " + tag + " Wait for logging")
    win = ctx['windows'].get(tag)
    assert win is not None
    win.wait_for_log(source=source, foldable=foldable, level=level, substr=substr)


def run_test_step_action_js_exec(ctx, step):
    print(get_indent(ctx) + "Action: " + step["action"])
    assert_browser(ctx)
    tag = step['window']
    cmd = step['cmd']
    print(get_indent(ctx) + "        " + tag + " Run " + cmd)
    win = ctx['windows'].get(tag)
    assert win is not None
    win.js_exec(cmd)


def run_test_step_action_quit(ctx, step):
    print(get_indent(ctx) + "Action: " + step["action"])
    assert_browser(ctx)
    browser = ctx.pop('browser')
    assert browser.quit_and_wait()


STEP_HANDLERS = {
    "launch":        run_test_step_action_launch,
    "window-new":    run_test_step_action_window_new,
    "window-close":  run_test_step_action_window_close,
    "navigate":      run_test_step_action_navigate,
    "stop":          run_test_step_action_stop,
    "sleep-ms":      run_test_step_action_sleep_ms,
    "block":         run_test_step_action_block,
    "repeat":        run_test_step_action_repeat,
    "timer-start":   run_test_step_action_timer_start,
    "timer-restart": run_test_step_action_timer_restart,
    "timer-stop":    run_test_step_action_timer_stop,
    "timer-check":   run_test_step_action_timer_check,
    "plot-check":    run_test_step_action_plot_check,
    "add-auth":      run_test_step_action_add_auth,
    "remove-auth":   run_test_step_action_remove_auth,
    "add-cert":      run_test_step_action_add_cert,
    "remove-cert":   run_test_step_action_remove_cert,
    "clear-log":     run_test_step_action_clear_log,
    "wait-log":      run_test_step_action_wait_log,
    "js-exec":       run_test_step_action_js_exec,
    "quit":          run_test_step_action_quit,
}


def run_test_step(ctx, step):
    STEP_HANDLERS[step["action"]](ctx, step)


def walk_test_plan(ctx, plan):
    ctx["depth"] = 0
    ctx["timers"] = dict()
    ctx['repeats'] = dict()
    for step in plan["steps"]:
        run_test_step(ctx, step)


def run_test_plan(ctx, plan):
    print_test_plan_info(ctx, plan)
    walk_test_plan(ctx, plan)


def run_preloaded_test(path_monkey, plan):
    ctx = {
        "monkey": path_monkey,
    }
    run_test_plan(ctx, plan)


def main(argv):
    ctx = {}
    path_monkey, path_test, wrapper = parse_argv(argv)
    plan = load_test_plan(path_test)
    ctx["monkey"] = path_monkey
    ctx["wrapper"] = wrapper
    run_test_plan(ctx, plan)


# Some python weirdness to get to main().
if __name__ == "__main__":
    main(sys.argv[1:])
