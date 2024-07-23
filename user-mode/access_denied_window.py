from tkinter import *
import customtkinter as ctk

DISPLAY_TIME = 3000  # displays this window for 3 seconds


class Gui:

    def __init__(self):
        """
        this function initializes and creates the gui window.
        """
        self.root = ctk.CTk()
        self.root.geometry('550x350')
        ctk.set_appearance_mode("dark")
        ctk.set_default_color_theme("dark-blue")
        self.label = ctk.CTkLabel(self.root, text='Access Denied', font=('Helvetica', 30))
        self.label.pack(pady=150)

    def close_window(self):
        self.root.withdraw()
        self.root.quit()


def main():
    gui = Gui()
    gui.root.overrideredirect(True)
    gui.root.attributes("-alpha", 0.97)  # set the window's opacity to less than 1, which means a little transparent
    gui.root.eval('tk::PlaceWindow . center')  # set the gui window to the center of the screen

    gui.root.after(DISPLAY_TIME, lambda: gui.close_window())  # close the window after 3 seconds

    gui.root.mainloop()

