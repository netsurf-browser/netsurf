#!/usr/bin/python3

import sys, getopt, yaml

from monkeyfarmer import Browser

def print_usage():
    print('Usage:')
    print('  ' + sys.argv[0] + ' -m <path to monkey> -t <path to test>')

def parse_argv(argv):
    path_monkey = ''
    path_test = ''
    try:
        opts, args = getopt.getopt(argv,"hm:t:",["monkey=","test="])
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

    if path_monkey == '':
        print_usage()
        sys.exit()
    if path_test == '':
        print_usage()
        sys.exit()

    return path_monkey, path_test

def load_test_plan(path):
    plan = []
    with open(path, 'r') as stream:
        try:
            plan = (yaml.load(stream))
        except:
            print (exc)
    return plan

def get_indent(ctx):
    return '  ' * ctx["depth"];

def print_test_plan_info(ctx, plan):
    print('Running test: [' + plan["group"] + '] ' + plan["title"])

def assert_browser(ctx):
    assert(ctx['browser'].started)
    assert(not ctx['browser'].stopped)
    
def run_test_step_action_launch(ctx, step):
    print(get_indent(ctx) + "Action: " + step["action"])
    assert(ctx.get('browser') is None)
    assert(ctx.get('windows') is None)
    ctx['browser'] = Browser(monkey_cmd=[ctx["monkey"]], quiet=True)
    assert_browser(ctx)
    ctx['windows'] = dict()

def run_test_step_action_window_new(ctx, step):
    print(get_indent(ctx) + "Action: " + step["action"])
    tag = step['tag']
    assert_browser(ctx)
    assert(ctx['windows'].get(tag) is None)
    ctx['windows'][tag] = ctx['browser'].new_window(url=step.get('url'))

def run_test_step_action_window_close(ctx, step):
    print(get_indent(ctx) + "Action: " + step["action"])
    assert_browser(ctx)
    tag = step['window']
    assert(ctx['windows'].get(tag) is not None)
    win = ctx['windows'].pop(tag)
    win.kill()
    win.wait_until_dead()
    assert(win.alive == False)

def run_test_step_action_navigate(ctx, step):
    print(get_indent(ctx) + "Action: " + step["action"])
    assert_browser(ctx)
    tag = step['window']
    win = ctx['windows'].get(tag)
    assert(win is not None)
    win.go(step['url'])

def run_test_step_action_sleep_ms(ctx, step):
    print(get_indent(ctx) + "Action: " + step["action"])

def run_test_step_action_block(ctx, step):
    print(get_indent(ctx) + "Action: " + step["action"])
    conds = step['conditions']
    assert_browser(ctx)

    def conds_met():
        for cond in conds:
            status = cond['status']
            window = cond['window']
            assert(status == "complete") # TODO: Add more status support?
            if window == "*all*":
                for win in ctx['windows'].items():
                    if win.throbbing:
                        return False
            else:
                win = ctx['windows'][window]
                if win.throbbing:
                    return False
        return True
    
    while not conds_met():
        ctx['browser'].farmer.loop(once=True)

def run_test_step_action_repeat(ctx, step):
    print(get_indent(ctx) + "Action: " + step["action"])
    ctx["depth"] += 1
    for step in step["steps"]:
        run_test_step(ctx, step)
    ctx["depth"] -= 1

def run_test_step_action_plot_check(ctx, step):
    print(get_indent(ctx) + "Action: " + step["action"])
    assert_browser(ctx)
    win = ctx['windows'][step['window']]
    checks = step['checks']
    all_text = []
    bitmaps = []
    for plot in win.redraw():
        if plot[0] == 'TEXT':
            all_text.extend(plot[6:])
        if plot[0] == 'BITMAP':
            bitmaps.append(plot[1:])
    all_text = " ".join(all_text)
    for check in checks:
        if 'text-contains' in check.keys():
            print("Check {} in {}".format(repr(check['text-contains']),repr(all_text)))
            assert(check['text-contains'] in all_text)
        elif 'bitmap-count' in check.keys():
            assert(len(bitmaps) == int(check['bitmap-count']))
        else:
            raise AssertionError("Unknown check: {}".format(repr(check)))

def run_test_step_action_timer_start(ctx, step):
    print(get_indent(ctx) + "Action: " + step["action"])

def run_test_step_action_timer_stop(ctx, step):
    print(get_indent(ctx) + "Action: " + step["action"])

def run_test_step_action_timer_check(ctx, step):
    print(get_indent(ctx) + "Action: " + step["action"])

def run_test_step_action_quit(ctx, step):
    print(get_indent(ctx) + "Action: " + step["action"])
    assert_browser(ctx)
    browser = ctx.pop('browser')
    windows = ctx.pop('windows')
    assert(browser.quit_and_wait())

step_handlers = {
    "launch":       run_test_step_action_launch,
    "window-new":   run_test_step_action_window_new,
    "window-close": run_test_step_action_window_close,
    "navigate":     run_test_step_action_navigate,
    "sleep-ms":     run_test_step_action_sleep_ms,
    "block":        run_test_step_action_block,
    "repeat":       run_test_step_action_repeat,
    "timer-start":  run_test_step_action_timer_start,
    "timer-stop":   run_test_step_action_timer_stop,
    "timer-check":  run_test_step_action_timer_check,
    "plot-check":   run_test_step_action_plot_check,
    "quit":         run_test_step_action_quit,
}

def run_test_step(ctx, step):
    step_handlers[step["action"]](ctx, step)

def walk_test_plan(ctx, plan):
    ctx["depth"] = 0
    for step in plan["steps"]:
        run_test_step(ctx, step)


def main(argv):
    ctx = {}
    path_monkey, path_test = parse_argv(argv)
    plan = load_test_plan(path_test)
    ctx["monkey"] = path_monkey
    print_test_plan_info(ctx, plan)
    walk_test_plan(ctx, plan)

# Some python weirdness to get to main().
if __name__ == "__main__":
    main(sys.argv[1:])
