// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// Copyright(C) 2005 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
// 02111-1307, USA.
//
// Graphical stuff related to the networking code:
//
//  * The client waiting screen when we are waiting for the server to
//    start the game.
//   

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "config.h"
#include "doomkeys.h"

#include "i_system.h"
#include "i_timer.h"
#include "i_video.h"

#include "net_client.h"
#include "net_gui.h"
#include "net_server.h"

#include "textscreen.h"

static txt_window_t *window;
static int old_max_players;
static txt_label_t *player_labels[NET_MAXPLAYERS];
static txt_label_t *ip_labels[NET_MAXPLAYERS];
static txt_label_t *drone_label;
static boolean had_warning;

static void EscapePressed(TXT_UNCAST_ARG(widget), void *unused)
{
    TXT_Shutdown();
    I_Quit();
}

static void StartGame(TXT_UNCAST_ARG(widget), TXT_UNCAST_ARG(settings))
{
    TXT_CAST_ARG(net_gamesettings_t, settings);

    NET_CL_StartGame(settings);
}

static void OpenWaitDialog(void)
{
    txt_window_action_t *cancel;

    TXT_SetDesktopTitle(PACKAGE_STRING);

    window = TXT_NewWindow("Waiting for game start...");

    TXT_AddWidget(window, TXT_NewLabel("\nPlease wait...\n\n"));

    cancel = TXT_NewWindowAction(KEY_ESCAPE, "Cancel");
    TXT_SignalConnect(cancel, "pressed", EscapePressed, NULL);

    TXT_SetWindowAction(window, TXT_HORIZ_LEFT, cancel);

    old_max_players = 0;
}

static void BuildWindow(void)
{
    char buf[50];
    txt_table_t *table;
    int i;

    TXT_ClearTable(window);
    table = TXT_NewTable(3);
    TXT_AddWidget(window, table);

    // Add spacers

    TXT_AddWidget(table, NULL);
    TXT_AddWidget(table, TXT_NewStrut(25, 1));
    TXT_AddWidget(table, TXT_NewStrut(17, 1));

    // Player labels

    for (i = 0; i < net_client_wait_data.max_players; ++i)
    {
        sprintf(buf, " %i. ", i + 1);
        TXT_AddWidget(table, TXT_NewLabel(buf));
        player_labels[i] = TXT_NewLabel("");
        ip_labels[i] = TXT_NewLabel("");
        TXT_AddWidget(table, player_labels[i]);
        TXT_AddWidget(table, ip_labels[i]);
    }

    drone_label = TXT_NewLabel("");

    TXT_AddWidget(window, drone_label);
}

static void UpdateGUI(net_gamesettings_t *settings)
{
    txt_window_action_t *startgame;
    char buf[50];
    unsigned int i;

    // If the value of max_players changes, we must rebuild the
    // contents of the window. This includes when the first
    // waiting data packet is received.

    if (net_client_received_wait_data)
    {
        if (net_client_wait_data.max_players != old_max_players)
        {
            BuildWindow();
        }
    }
    else
    {
        return;
    }

    for (i = 0; i < net_client_wait_data.max_players; ++i)
    {
        txt_color_t color = TXT_COLOR_BRIGHT_WHITE;

        if ((signed) i == net_client_wait_data.consoleplayer)
        {
            color = TXT_COLOR_YELLOW;
        }

        TXT_SetFGColor(player_labels[i], color);
        TXT_SetFGColor(ip_labels[i], color);

        if (i < net_client_wait_data.num_players)
        {
            TXT_SetLabel(player_labels[i],
                         net_client_wait_data.player_names[i]);
            TXT_SetLabel(ip_labels[i],
                         net_client_wait_data.player_addrs[i]);
        }
        else
        {
            TXT_SetLabel(player_labels[i], "");
            TXT_SetLabel(ip_labels[i], "");
        }
    }

    if (net_client_wait_data.num_drones > 0)
    {
        sprintf(buf, " (+%i observer clients)",
                     net_client_wait_data.num_drones);
        TXT_SetLabel(drone_label, buf);
    }
    else
    {
        TXT_SetLabel(drone_label, "");
    }

    if (net_client_wait_data.is_controller)
    {
        startgame = TXT_NewWindowAction(' ', "Start game");
        TXT_SignalConnect(startgame, "pressed", StartGame, settings);
    }
    else
    {
        startgame = NULL;
    }

    TXT_SetWindowAction(window, TXT_HORIZ_RIGHT, startgame);
}

static void PrintMD5Digest(char *s, byte *digest)
{
    unsigned int i;

    printf("%s: ", s);

    for (i=0; i<sizeof(md5_digest_t); ++i)
    {
        printf("%02x", digest[i]);
    }

    printf("\n");
}

static void CheckMD5Sums(void)
{
    boolean correct_wad, correct_deh;
    boolean same_freedoom;
    txt_window_t *window;

    if (!net_client_received_wait_data || had_warning)
    {
        return;
    }

    correct_wad = memcmp(net_local_wad_md5sum,
                         net_client_wait_data.wad_md5sum, 
                         sizeof(md5_digest_t)) == 0;
    correct_deh = memcmp(net_local_deh_md5sum,
                         net_client_wait_data.deh_md5sum, 
                         sizeof(md5_digest_t)) == 0;
    same_freedoom = net_client_wait_data.is_freedoom == net_local_is_freedoom;

    if (correct_wad && correct_deh && same_freedoom)
    {
        return;
    }

    if (!correct_wad)
    {
        printf("Warning: WAD MD5 does not match server:\n");
        PrintMD5Digest("Local", net_local_wad_md5sum);
        PrintMD5Digest("Server", net_client_wait_data.wad_md5sum);
    }

    if (!same_freedoom)
    {
        printf("Warning: Mixing Freedoom with non-Freedoom\n");
        printf("Local: %i  Server: %i\n", 
               net_local_is_freedoom, 
               net_client_wait_data.is_freedoom);
    }

    if (!correct_deh)
    {
        printf("Warning: Dehacked MD5 does not match server:\n");
        PrintMD5Digest("Local", net_local_deh_md5sum);
        PrintMD5Digest("Server", net_client_wait_data.deh_md5sum);
    }

    window = TXT_NewWindow("WARNING");

    TXT_SetWindowAction(window, TXT_HORIZ_RIGHT, NULL);

    if (!same_freedoom)
    {
        // If Freedoom and Doom IWADs are mixed, the WAD directory
        // will be wrong, but this is not neccessarily a problem.
        // Display a different message to the WAD directory message.

        if (net_local_is_freedoom)
        {
            TXT_AddWidget(window, TXT_NewLabel
            ("You are using the Freedoom IWAD to play with players\n"
             "using an official Doom IWAD.  Make sure that you are\n"
             "playing the same levels as other players.\n"));
        }
        else
        {
            TXT_AddWidget(window, TXT_NewLabel
            ("You are using an official IWAD to play with players\n"
             "using the Freedoom IWAD.  Make sure that you are\n"
             "playing the same levels as other players.\n"));
        }
    }
    else if (!correct_wad)
    {
        TXT_AddWidget(window, TXT_NewLabel
            ("Your WAD directory does not match other players in the game.\n"
             "Check that you have loaded the exact same WAD files as other\n"
             "players.\n"));
    }

    if (!correct_deh)
    {
        TXT_AddWidget(window, TXT_NewLabel
            ("Your dehacked signature does not match other players in the\n"
             "game.  Check that you have loaded the same dehacked patches\n"
             "as other players.\n"));
    }

    TXT_AddWidget(window, TXT_NewLabel
            ("If you continue, this may cause your game to desync."));
    
    had_warning = true;
}

void NET_WaitForStart(net_gamesettings_t *settings)
{
    if (!TXT_Init())
    {
        fprintf(stderr, "Failed to initialize GUI\n");
        exit(-1);
    }

    I_SetWindowTitle("Waiting for game start");
    //I_SetWindowIcon();

    OpenWaitDialog();
    had_warning = false;

    while (net_waiting_for_start)
    {
        UpdateGUI(settings);
        CheckMD5Sums();

        TXT_DispatchEvents();
        TXT_DrawDesktop();

        NET_CL_Run();
        NET_SV_Run();

        if (!net_client_connected)
        {
            I_Error("Lost connection to server");
        }

        TXT_Sleep(100);
    }
    
    TXT_Shutdown();
}
