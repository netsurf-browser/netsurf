#!/usr/bin/python3

import sys, getopt, yaml

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

def run_test_step_action_launch(ctx, step):
    print(get_indent(ctx) + "Action: " + step["action"])

def run_test_step_action_window_new(ctx, step):
    print(get_indent(ctx) + "Action: " + step["action"])

def run_test_step_action_window_close(ctx, step):
    print(get_indent(ctx) + "Action: " + step["action"])

def run_test_step_action_navigate(ctx, step):
    print(get_indent(ctx) + "Action: " + step["action"])

def run_test_step_action_sleep_ms(ctx, step):
    print(get_indent(ctx) + "Action: " + step["action"])

def run_test_step_action_block(ctx, step):
    print(get_indent(ctx) + "Action: " + step["action"])

def run_test_step_action_repeat(ctx, step):
    print(get_indent(ctx) + "Action: " + step["action"])
    ctx["depth"] += 1
    for step in step["steps"]:
        run_test_step(ctx, step)
    ctx["depth"] -= 1

def run_test_step_action_timer_start(ctx, step):
    print(get_indent(ctx) + "Action: " + step["action"])

def run_test_step_action_timer_stop(ctx, step):
    print(get_indent(ctx) + "Action: " + step["action"])

def run_test_step_action_timer_check(ctx, step):
    print(get_indent(ctx) + "Action: " + step["action"])

def run_test_step_action_quit(ctx, step):
    print(get_indent(ctx) + "Action: " + step["action"])

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
    print_test_plan_info(ctx, plan)
    walk_test_plan(ctx, plan)

# Some python weirdness to get to main().
if __name__ == "__main__":
    main(sys.argv[1:])