with open("kernel_main_linux.cpp", "r") as f:
    content = f.read()

target = """                                        windows[0].content[c_idx] = 0; windows[0].cursor_pos = c_idx;
                                    }
                                }
                                input_cooldown = 25;
                            }"""

replacement = """                                        windows[0].content[c_idx] = 0; windows[0].cursor_pos = c_idx;
                                    }
                                }
                                }
                                input_cooldown = 25;
                            }"""

content = content.replace(target, replacement)

with open("kernel_main_linux.cpp", "w") as f:
    f.write(content)
