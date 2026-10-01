import asyncio

import threading

import tkinter as tk

from tkinter import ttk



from bleak import BleakClient, BleakScanner





# ============================================================

# KONFIGURATION

# ============================================================



DEVICE_NAME = "Kontorsskylt"



SERVICE_UUID = "7d230001-5475-4a28-a975-8a03cafe0001"



CHARACTERISTIC_UUID = "7d230002-5475-4a28-a975-8a03cafe0001"

STATE_CHARACTERISTIC_UUID = "7d230003-5475-4a28-a975-8a03cafe0001"



MAX_TEXT_BYTES = 120



COLORS = [

    "WHITE",

    "BLACK",

    "RED",

]





# ============================================================

# STATUS

# ============================================================



def set_status(text):

    """

    Uppdatera statusfältet på ett Tkinter-säkert sätt.



    BLE körs i en separat tråd, så GUI-ändringar skickas

    tillbaka till Tkinter-tråden med root.after().

    """



    root.after(

        0,

        lambda: status_var.set(text)

    )





# ============================================================

# CUSTOM-VALIDERING

# ============================================================




# ============================================================
# SKYLTENS BEKRÄFTADE LÄGE
# ============================================================

def set_display_state(text, background="WHITE", foreground="BLACK"):
    root.after(
        0,
        lambda: apply_display_state(text, background, foreground)
    )


def apply_display_state(text, background, foreground):
    display_state_var.set(text)

    color_map = {
        "WHITE": "white",
        "BLACK": "black",
        "RED": "#c62828",
    }

    state_value_label.configure(
        background=color_map.get(background, "white"),
        foreground=color_map.get(foreground, "black"),
    )


def parse_display_state(raw_state):
    if raw_state == "STATE|OFFICE":
        return "PÅ KONTORET", "WHITE", "BLACK"

    if raw_state == "STATE|HOME":
        return "ARBETAR HEMIFRÅN", "RED", "WHITE"

    prefix = "STATE|CUSTOM|BG="
    fg_marker = "|FG="
    text_marker = "|TEXT="

    if raw_state.startswith(prefix):
        fg_pos = raw_state.find(fg_marker, len(prefix))
        if fg_pos == -1:
            raise ValueError(f"Ogiltigt STATE-värde: {raw_state}")

        text_pos = raw_state.find(text_marker, fg_pos + len(fg_marker))
        if text_pos == -1:
            raise ValueError(f"Ogiltigt STATE-värde: {raw_state}")

        background = raw_state[len(prefix):fg_pos]
        foreground = raw_state[fg_pos + len(fg_marker):text_pos]
        text = raw_state[text_pos + len(text_marker):]

        return text, background, foreground

    raise ValueError(f"Okänt STATE-värde: {raw_state}")


async def read_display_state(client):
    raw = await client.read_gatt_char(STATE_CHARACTERISTIC_UUID)
    state = bytes(raw).decode("utf-8", errors="replace")

    text, background, foreground = parse_display_state(state)
    set_display_state(text, background, foreground)

    return state


def validate_custom_text(text, background, foreground):

    if not text.strip():

        return False, "Texten får inte vara tom."



    if background == foreground:

        return False, "Bakgrund och textfärg får inte vara samma."



    text_bytes = text.encode("utf-8")



    if len(text_bytes) > MAX_TEXT_BYTES:

        return (

            False,

            f"Texten är för lång ({len(text_bytes)} av "

            f"{MAX_TEXT_BYTES} byte)."

        )



    return True, ""





def build_custom_command(text, background, foreground):

    return (

        f"CUSTOM|BG={background}|"

        f"FG={foreground}|"

        f"TEXT={text}"

    )





# ============================================================

# BLE

# ============================================================



async def send_ble_command(command):
    set_status("Söker efter Kontorsskylt...")

    device = await BleakScanner.find_device_by_name(
        DEVICE_NAME,
        timeout=10.0
    )

    if device is None:
        set_status("Fel: Kontorsskylt hittades inte")
        return

    set_status("Ansluter...")

    async with BleakClient(device) as client:
        try:
            await read_display_state(client)
        except Exception as error:
            print(f"Kunde inte läsa STATE före kommandot: {error}")

        final_event = asyncio.Event()
        final_result = {"value": None}

        def notification_handler(sender, data):
            message = bytes(data).decode("utf-8", errors="replace")
            print(f"BLE notification: {message}")

            if message == "OK|QUEUED":
                return

            if message.startswith("OK|") or message.startswith("ERR|"):
                final_result["value"] = message
                final_event.set()

        await client.start_notify(
            CHARACTERISTIC_UUID,
            notification_handler
        )

        set_status("Skickar kommando...")

        await client.write_gatt_char(
            CHARACTERISTIC_UUID,
            command.encode("utf-8"),
            response=False
        )

        set_status("Skylten uppdateras...")

        try:
            await asyncio.wait_for(final_event.wait(), timeout=30.0)
        except asyncio.TimeoutError:
            set_status("Fel: ingen slutbekräftelse från skylten")
            return
        finally:
            try:
                await client.stop_notify(CHARACTERISTIC_UUID)
            except Exception:
                pass

        result = final_result["value"]

        if result is None:
            set_status("Fel: okänt BLE-svar")
            return

        if result.startswith("ERR|"):
            set_status(f"Fel från skylten: {result}")
            return

        try:
            await read_display_state(client)
        except Exception as error:
            set_status(
                "Displayen uppdaterades, men STATE kunde inte läsas: "
                f"{error}"
            )
            return

    set_status("Klar")


def ble_worker(command):

    try:

        asyncio.run(

            send_ble_command(command)

        )



    except Exception as error:

        set_status(

            f"Fel: {error}"

        )





def send_command(command):

    threading.Thread(

        target=ble_worker,

        args=(command,),

        daemon=True

    ).start()





# ============================================================

# OFFICE

# ============================================================



def send_office():

    send_command(

        "OFFICE"

    )





# ============================================================

# HOME

# ============================================================



def send_home():

    send_command(

        "HOME"

    )





# ============================================================

# CUSTOM

# ============================================================



def send_custom():

    text = custom_text_var.get()



    background = background_var.get()



    foreground = foreground_var.get()



    valid, error_message = validate_custom_text(

        text,

        background,

        foreground

    )



    if not valid:

        set_status(

            f"Fel: {error_message}"

        )

        return



    command = build_custom_command(

        text,

        background,

        foreground

    )



    send_command(

        command

    )





# ============================================================

# GUI

# ============================================================



root = tk.Tk()



root.title(

    "Kontorsskylt"

)



root.resizable(

    True,

    True

)





# ============================================================

# HUVUDRAM

# ============================================================



main_frame = ttk.Frame(

    root,

    padding=20

)



main_frame.grid(

    row=0,

    column=0,

    sticky="nsew"

)



root.columnconfigure(

    0,

    weight=1

)



root.rowconfigure(

    0,

    weight=1

)



main_frame.columnconfigure(

    0,

    weight=1

)



main_frame.columnconfigure(

    1,

    weight=1

)





# ============================================================

# RUBRIK

# ============================================================



title_label = ttk.Label(

    main_frame,

    text="Kontorsskylt",

    font=(

        "TkDefaultFont",

        18,

        "bold"

    )

)



title_label.grid(

    row=0,

    column=0,

    columnspan=2,

    pady=(

        0,

        20

    )

)





# ============================================================

# OFFICE / HOME

# ============================================================



office_button = ttk.Button(

    main_frame,

    text="På kontoret",

    command=send_office

)



office_button.grid(

    row=1,

    column=0,

    padx=5,

    pady=5,

    sticky="ew"

)





home_button = ttk.Button(

    main_frame,

    text="Arbetar hemifrån",

    command=send_home

)



home_button.grid(

    row=1,

    column=1,

    padx=5,

    pady=5,

    sticky="ew"

)





# ============================================================

# SEPARATOR

# ============================================================



separator = ttk.Separator(

    main_frame,

    orient="horizontal"

)



separator.grid(

    row=2,

    column=0,

    columnspan=2,

    sticky="ew",

    pady=20

)





# ============================================================

# CUSTOM RUBRIK

# ============================================================



custom_label = ttk.Label(

    main_frame,

    text="Egen text",

    font=(

        "TkDefaultFont",

        12,

        "bold"

    )

)



custom_label.grid(

    row=3,

    column=0,

    columnspan=2,

    pady=(

        0,

        10

    )

)





# ============================================================

# TEXTFÄLT

# ============================================================



custom_text_var = tk.StringVar()



custom_entry = ttk.Entry(

    main_frame,

    textvariable=custom_text_var

)



custom_entry.grid(

    row=4,

    column=0,

    columnspan=2,

    padx=5,

    pady=5,

    sticky="ew"

)





# ============================================================

# BAKGRUNDSFÄRG

# ============================================================



background_label = ttk.Label(

    main_frame,

    text="Bakgrund"

)



background_label.grid(

    row=5,

    column=0,

    padx=5,

    pady=(

        15,

        2

    )

)





background_var = tk.StringVar(

    value="WHITE"

)



background_combo = ttk.Combobox(

    main_frame,

    textvariable=background_var,

    values=COLORS,

    state="readonly"

)



background_combo.grid(

    row=6,

    column=0,

    padx=5,

    pady=5,

    sticky="ew"

)





# ============================================================

# TEXTFÄRG

# ============================================================



foreground_label = ttk.Label(

    main_frame,

    text="Textfärg"

)



foreground_label.grid(

    row=5,

    column=1,

    padx=5,

    pady=(

        15,

        2

    )

)





foreground_var = tk.StringVar(

    value="BLACK"

)



foreground_combo = ttk.Combobox(

    main_frame,

    textvariable=foreground_var,

    values=COLORS,

    state="readonly"

)



foreground_combo.grid(

    row=6,

    column=1,

    padx=5,

    pady=5,

    sticky="ew"

)





# ============================================================

# SKICKA CUSTOM

# ============================================================



custom_button = ttk.Button(

    main_frame,

    text="Visa egen text",

    command=send_custom

)



custom_button.grid(

    row=7,

    column=0,

    columnspan=2,

    padx=5,

    pady=(

        15,

        5

    ),

    sticky="ew"

)





# ============================================================

# STATUS

# ============================================================




# ============================================================
# SKYLTEN VISAR NU
# ============================================================

state_separator = ttk.Separator(
    main_frame,
    orient="horizontal"
)

state_separator.grid(
    row=8,
    column=0,
    columnspan=2,
    sticky="ew",
    pady=(20, 15)
)


state_title_label = ttk.Label(
    main_frame,
    text="SKYLTEN VISAR NU",
    anchor="center",
    font=("TkDefaultFont", 10, "bold")
)

state_title_label.grid(
    row=9,
    column=0,
    columnspan=2,
    padx=5,
    pady=(0, 8),
    sticky="ew"
)


display_state_var = tk.StringVar(value="Inte läst ännu")

state_value_label = tk.Label(
    main_frame,
    textvariable=display_state_var,
    anchor="center",
    justify="center",
    font=("TkDefaultFont", 14, "bold"),
    padx=15,
    pady=15,
    relief="solid",
    borderwidth=1,
    background="white",
    foreground="black"
)

state_value_label.grid(
    row=10,
    column=0,
    columnspan=2,
    padx=5,
    pady=(0, 5),
    sticky="ew"
)


status_var = tk.StringVar(

    value="Redo"

)



status_label = ttk.Label(

    main_frame,

    textvariable=status_var,

    anchor="center"

)



status_label.grid(

    row=11,

    column=0,

    columnspan=2,

    padx=5,

    pady=(

        20,

        0

    ),

    sticky="ew"

)





# ============================================================

# START

# ============================================================




# ============================================================
# LÄS SKYLTENS LÄGE VID PROGRAMSTART
# ============================================================

async def load_initial_display_state():
    set_status("Söker efter Kontorsskylt...")

    device = await BleakScanner.find_device_by_name(
        DEVICE_NAME,
        timeout=10.0
    )

    if device is None:
        set_status("Redo - skylten hittades inte")
        return

    set_status("Läser skyltens läge...")

    async with BleakClient(device) as client:
        await read_display_state(client)

    set_status("Redo")


def initial_state_worker():
    try:
        asyncio.run(load_initial_display_state())
    except Exception as error:
        set_status(f"Redo - kunde inte läsa skyltläge: {error}")


def load_initial_state():
    threading.Thread(
        target=initial_state_worker,
        daemon=True
    ).start()


custom_entry.focus()



root.after(
    250,
    load_initial_state
)

root.mainloop()