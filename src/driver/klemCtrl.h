/*
 * Wireless Kernel Link Emulator
 *
 * Copyright (C) 2013 - 2026 Stuart Wells <swells@stuartwells.net>
 *
 * Licensed under the GNU Lesser General Public License, version 2.1 (LGPLv2.1)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 */
#ifndef KLEM_CTRL_INCLUDE
#define KLEM_CTRL_INCLUDE

void klemCtrlCreate(void *pPtr);
void klemCtrlStart(void *pPtr);
void klemCtrlStop(void *pPtr);
void klemCtrlDestroy(void *pPtr);
#endif
