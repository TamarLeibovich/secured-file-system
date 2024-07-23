import customtkinter as ctk
from tkinter import *

password_input = ''


class Gui:

    def __init__(self):
        """
        this function initializes and creates the gui window.
        """
        self.root = ctk.CTk()
        self.root.geometry('550x350')
        ctk.set_appearance_mode("dark")
        ctk.set_default_color_theme("dark-blue")
        self.label = ctk.CTkLabel(self.root, text='Initialize Password', font=('Helvetica', 30))
        self.label.pack(pady=40)
        self.password_entry = self.create_entry()
        self.password_entry.pack(pady=20)
        self.submit_button = self.create_button()
        self.submit_button.pack(pady=10)

    def submit_password(self):
        """
        this function is called when the submit button has been pressed. It assigns to the global variable
        password_input the password entered in password_entry, closes the gui window and disable the submit button.
        """
        global password_input
        password_input = self.password_entry.get()
        self.password_entry.delete(0, END)
        self.submit_button.configure(state='disabled')

        self.root.withdraw()
        self.root.quit()

    def create_button(self):
        """
        this function creates the button Submit.
        """
        return ctk.CTkButton(self.root, text='Submit', command=self.submit_password, height=65, width=180,
                             font=('Helvetica', 20))

    def create_entry(self):
        """
        this function creates the entry box which the user will insert a password in.
        """
        return ctk.CTkEntry(self.root, placeholder_text='Enter Password', height=50, width=300,
                            font=('Helvetica', 15), show='\u25CF')


def main():
    gui = Gui()
    gui.root.overrideredirect(True)
    gui.root.attributes("-alpha", 0.97)  # set the window's opacity to less than 1, which means a little transparent
    gui.root.eval('tk::PlaceWindow . center')  # set the gui window to the center of the screen

    gui.root.mainloop()
    return password_input
