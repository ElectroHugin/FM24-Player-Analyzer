# FM Dashboard - Development TODO List

## Category 1: High-Impact User Experience & Quality of Life

- [ ] **Conditional Formatting in Data Tables**
    - **What it is:** Use colors, icons, and data bars directly within dataframes (e.g., Squad Matrix, Role Analysis) to make key information visually stand out.
    - **Why it's useful:** Allows for at-a-glance analysis of player strengths and weaknesses without reading every number. A major visual and usability upgrade.
    - **Implementation Notes:** Utilize `st.dataframe`'s `column_config` parameter.
        - Use `st.column_config.ProgressColumn` for DWRS ratings (0-100%).
        - Use `st.column_config.NumberColumn` for attributes (1-20) and apply custom CSS or styling functions if needed for red/yellow/green color scales.

- [ ] **Global Player Search in Sidebar**
    - **What it is:** A persistent search bar in the sidebar that allows users to find any player in the database from anywhere in the app.
    - **Why it's useful:** Drastically improves navigation speed and accessibility of player data.
    - **Implementation Notes:**
        - Add an `st.text_input` to `sidebar()`.
        - As the user types, filter the results from `get_all_players()`.
        - Display a clickable list of results directly below the search bar in the sidebar. Clicking a result could navigate to a dedicated "Player Profile" page or open a modal.

- [ ] **In-Context Editing with Modals (`st.dialog`)**
    - **What it is:** Instead of navigating to the "Edit Player" page for minor changes, add small "edit" icons next to players in key tables (like the Squad Matrix). Clicking the icon opens a `st.dialog` pop-up to modify data like "Primary Role" or "Agreed Playing Time" directly.
    - **Why it's useful:** Keeps the user in their analytical flow, allowing for quick adjustments and immediate feedback without context switching.
    - **Implementation Notes:** Use the `st.dialog` feature. The button/icon click opens the dialog containing a small form. On submission, the form updates the database and reruns the page.

---

## Category 2: Deeper Analytical Features

- [ ] **"Pros & Cons" Analysis for Roles**
    - **What it is:** When viewing a player's suitability for a role, automatically generate a bulleted list of their key strengths and weaknesses *for that specific role*.
    - **Why it's useful:** Provides a human-readable explanation behind the DWRS number. Translates raw attributes into actionable insights (e.g., "Pro: Elite Finishing for a Poacher", "Con: Low Work Rate for a Pressing Forward").
    - **Implementation Notes:**
        - Create a helper function that takes a player and a role.
        - Compare the player's attributes against the role's "Key" and "Preferable" attributes from `definitions.json`.
        - Use thresholds (e.g., attribute > 15 is a "Pro", < 10 is a "Con") to build the lists.

- [x] **Youth Development Squad View**
    - **What it is:** On the "Best XI" page, add a tab, checkbox, or selectbox to calculate a "Youth XI". This would run the Best XI algorithm using only players who are at or below the configured youth age thresholds.
    - **Why it's useful:** Essential for long-term saves and succession planning. It provides a clear view of the club's future and highlights gaps in youth development.
    - **Implementation Notes:** In `best_position_calculator_page`, add a UI element to trigger this mode. If active, filter the `my_club_players` list by age before passing it to `calculate_squad_and_surplus`. The `get_age_threshold` function is already available.

- [ ] **Data Management: Settings Export/Import**
    - **What it is:** Add "Export" and "Import" buttons to the Settings page. Export bundles `config.ini` and `definitions.json` into a `.zip` file. Import allows a user to upload that `.zip` to apply a full configuration.
    - **Why it's useful:** Enables users to back up their personalized setup, migrate it to another machine, or share their tactical systems (weights, custom roles, etc.) with others.
    - **Implementation Notes:** Use Python's built-in `zipfile` library. `st.download_button` for exporting and `st.file_uploader` for importing.

---

## Category 3: Advanced Player Personalization & Positional Intelligence

- [x] **Step 1: Database & Data Layer Update**
    - **What it is:** Enhance the database and data pipeline to store "Preferred Foot" and a new manually-assigned "Natural Positions" field for each player.
    - **Why it's useful:** This is the foundational step to enable more nuanced tactical calculations.
    - **Implementation Notes:**
        - **Schema:** In `sqlite_db.py`, add `ALTER TABLE` commands in `init_db()` to create a `preferred_foot` (TEXT) column and a `natural_positions` (TEXT) column.
        - **Data Import:** In `data_parser.py`, when processing the HTML, check the "Left Foot" and "Right Foot" columns to determine the `preferred_foot` (e.g., 'Right', 'Left', 'Either').
        - **Manual Input:** On the "Edit Player" page (`edit_player_data_page`), add a `st.multiselect` widget allowing the user to select one or more "Natural Positions" for a player from the `MASTER_POSITION_MAP`. Store this as a JSON string or comma-separated list in the database.

- [x] **Step 2: Enhance the Best XI Algorithm**
    - **What it is:** Update the `calculate_squad_and_surplus` logic to use the new `preferred_foot` and `natural_positions` data, adding a new layer of intelligence to player selection.
    - **Why it's useful:** Makes the Best XI more realistic by rewarding players for playing in their most comfortable positions and using foot preference as a smart tie-breaker for symmetrical roles.
    - **Implementation Notes:**
        - **Natural Position Bonus:**
            - Add a `natural_position_multiplier` to your `config.ini` (e.g., default `1.05` for a 5% bonus).
            - In `squad_logic.py`, when calculating the `selection_score`, check if the position being evaluated (`pos`) is in the player's list of `natural_positions`.
            - The new formula would be: `selection_score = rating * apt_weight * natural_pos_bonus`.
        - **Preferred Foot Tie-Breaker:**
            - This logic should be applied subtly, only when scores are very close.
            - When comparing candidates, if two players have nearly identical scores for symmetrical positions (e.g., `DCL`/`DCR`, `ML`/`MR`), apply a tiny bonus.
            - Example logic: If `pos == 'DCR'` and `player.preferred_foot == 'Right'`, `selection_score *= 1.001`. If `pos == 'DCL'` and `player.preferred_foot == 'Left'`, `selection_score *= 1.001`. This ensures it only breaks ties and doesn't override a significantly better player.


# UI & "Flavor" Enhancement Ideas

## Category 1: Establishing Club Identity & Context

These ideas build on your suggestion to add the full club name, creating a strong sense of place for the user.

- [x] **A Dynamic and Personalized Dashboard Header**
    - **What it is:** Replace the static `st.title("Player Dashboard")` on the main page with a dynamic header component. This header would appear at the top of every page.
    - **How it would look:**
        - Use `st.columns` to create a three-part header.
        - **Left:** The uploaded club logo (`st.image`).
        - **Center:** The full club name (from a new setting) as the main title, with a dynamic subtitle underneath like *"Analyzing Save: MyFM24Save.db"*.
        - **Right:** Maybe the current in-app date or a small status indicator.
    - **Why it adds flavor:** It immediately brands the entire application for the user's club and reminds them which save file they are currently analyzing. It feels professional and bespoke.

- [x] **Add "Full Club Name" to Settings**
    - **What it is:** A new text input field in the Settings page under "Club Identity & Theme" for the user to enter their club's full name (e.g., "Manchester United" instead of just "Man UFC").
    - **Why it's needed:** This provides the data for the dynamic header.
    - **Implementation:** Store this in `sqlite_db` in the `settings` table, similar to how `user_club` is stored.

## Category 2: Improving UI Layout & Information Hierarchy

These ideas focus on restructuring how information is presented to make the app feel cleaner and more intuitive, reducing the need for excessive scrolling.

- [ ] **Leverage Tabs for Cleaner Pages (`st.tabs`)**
    - **What it is:** On pages with multiple distinct sections, organize them into tabs instead of a long vertical list.
    - **Prime Candidates:**
        - **Best XI Page:** Instead of scrolling, have tabs for `[Starting XI]`, `[B-Team]`, and `[Depth & Surplus]`.
        - **Role Analysis Page:** Use tabs for `[My Club]`, `[Second Team]`, and `[Scouted Players]`.
    - **Why it adds flavor:** It makes the application feel more like a structured software and less like a simple script. It dramatically cleans up the UI and improves discoverability of information.

- [ ] **Create a Dedicated "Player Profile" View**
    - **What it is:** When a user clicks on a player's name anywhere in the app (or via a global search), it takes them to a dedicated page or opens a full-screen modal (`st.dialog`) that presents a complete overview of that player.
    - **Contents:**
        - Player's name, age, club, and a dynamic title like "Star Player" or "Breakthrough Prospect".
        - Their top 3-5 roles with their DWRS ratings.
        - The "Pros & Cons" analysis for their best role.
        - A compact view of their key attributes using `st.metric` or colored tags.
        - The DWRS development chart for their top roles.
    - **Why it adds flavor:** It creates a central, authoritative source of information for each player, making analysis feel more focused and powerful.

## Category 3: Component-Level Polish & Data Storytelling

These are smaller, subtle changes that add a layer of polish and help the data tell a more compelling story.

- [ ] **Use Icons to Add Visual Cues**
    - **What it is:** Add small, relevant emojis or icons to subheaders and buttons.
    - **Examples:**
        - `st.subheader("🏆 Best XI")`
        - `st.subheader("📈 Player Development")`
        - `st.button("💾 Save All Settings")`
        - Highlighting the top-ranked player in a list with a 🥇 icon.
    - **Why it adds flavor:** It breaks up the monotony of text, adds visual interest without being tacky, and helps users mentally categorize information faster.

- [ ] **Create Custom "Status Tags"**
    - **What it is:** When displaying qualitative data like "Agreed Playing Time" or a player's transfer status, use a custom-styled HTML "tag" or "pill" instead of plain text.
    - **Example:** Instead of "Agreed Playing Time: Star Player", you would see a colored tag that says **Star Player**.
    - **Why it adds flavor:** This is a classic UI pattern from modern web apps (like GitHub labels). It's visually appealing and conveys information efficiently. You can create a helper function that generates the HTML `<span>` with the correct color based on the status.

- [ ] **Write "Manager's Insights"**
    - **What it is:** After a complex calculation (like the Best XI), use an `st.info` or `st.success` block to provide a simple, human-readable summary of the results.
    - **Example:** After calculating surplus players: `st.info("💡 Manager's Insight: Your squad has excellent depth at Center Back but lacks cover for the Left Wing-Back role.")`
    - **Why it adds flavor:** It makes the app feel like an intelligent assistant rather than just a calculator. It bridges the gap between raw data and actionable strategy.



# Development Roadmap & Feature Ideas for FM Player Analysis Dashboard

Here is a collection of proposed features and enhancements to continue the development of the application, building upon its strong foundation.

## 🚀 Proposed New Features

### 1. Scouting & Shortlist Management Page
Create a dedicated hub for tracking, shortlisting, and managing scouted players to streamline transfer target identification.

*   **Functionality:**
    *   A main table view showing all players who are not part of your club.
    *   An "Add to Shortlist" button or checkbox next to each player.
    *   A separate tab or filtered view on the page to display only shortlisted players.
    *   The ability to add custom text notes and a "Scout Rating" (e.g., 1-5 stars) to each shortlisted player.
*   **Implementation Steps:**
    1.  **Database:** Add new columns to the `players` table in `sqlite_db.py`: `is_shortlisted` (INTEGER/BOOLEAN), `scout_notes` (TEXT), and `scout_rating` (INTEGER).
    2.  **UI:** Create a new page function in `app.py` called `scouting_page()`.
    3.  **Logic:** This page will use `get_all_players()` to fetch and filter the scouted players.
    4.  **Database Interaction:** Implement new functions in `sqlite_db.py` to update the new columns (e.g., `update_shortlist_status()`, `update_scout_notes()`).

### 2. Financial Overview Page
Aggregate the existing `Wage` and `Transfer Value` data to provide valuable financial insights about your squad.

*   **Functionality:**
    *   Display key dashboard stats for your club: Total Wage Bill (per week/month), Average Wage, and Total Squad Market Value.
    *   A bar chart visualizing the wage distribution across the squad (e.g., how many players are in the £0-5k, £5-10k brackets).
    *   A scatter plot of Player Age vs. Transfer Value to identify players who may be peaking in value or are undervalued.
    *   A simple table showing the highest earners and most valuable players.
*   **Implementation Steps:**
    1.  **UI:** Create a `financial_overview_page()` in `app.py`.
    2.  **Data Parsing:** Create a new utility function to parse financial strings (e.g., "£5.5K p/w", "$1.2M") into numerical float values for calculation.
    3.  **Logic:** Load your club's players and perform the necessary aggregations (sum, average).
    4.  **Visualization:** Use Plotly or Streamlit's native charting functions (`st.bar_chart`, `st.scatter_chart`) to create the visualizations.

### 3. Head-to-Head Team Comparison
Allow users to compare the quality of their starting XI against any other team in the database for a specific tactic.

*   **Functionality:**
    *   Two select boxes: "Team A" (defaults to your club) and "Team B".
    *   A select box for the tactic to use for the comparison.
    *   The app will calculate the best Starting XI for both teams using the existing `calculate_squad_and_surplus` logic.
    *   Display the two tactical grids side-by-side for a direct visual comparison.
    *   Show a summary table comparing the average DWRS for each position (e.g., Your Striker: 85% vs. Their Striker: 82%).
*   **Implementation Steps:**
    1.  **UI:** Create a `team_comparison_page()` in `app.py`.
    2.  **Core Logic:** The page will run the `calculate_squad_and_surplus` function twice, once for each selected club's player pool.
    3.  **Presentation:** Use `st.columns(2)` to display the tactic grids side-by-side and an `st.dataframe` for the final positional breakdown.

## ✨ UI/UX & Quality-of-Life Enhancements

Smaller but impactful changes focused on improving the user experience and workflow.

*   **Persistent Filters:**
    *   **Goal:** User filter selections on pages like "Assign Roles" should be remembered during the session.
    *   **Implementation:** You have already correctly implemented this using `st.session_state`. This pattern can be expanded to other pages with filters, such as the "Player-Role Matrix" or the proposed "Scouting" page.

*   **Advanced Player Search:**
    *   **Goal:** Go beyond the current name-only search to allow for more complex queries.
    *   **Implementation:** Add a filtering component (perhaps in an `st.expander`) that allows users to filter by age range, position, or even specific attributes (e.g., show all players with "Pace > 15" and "Finishing > 14").

*   **Data Export (CSV):**
    *   **Goal:** Allow users to download the data from tables for offline analysis.
    *   **Implementation:** On pages with data tables like "Role Analysis" and "Squad Matrix", add an `st.download_button`. The data from the DataFrame can be easily converted to a CSV string using `df.to_csv(index=False)`.

*   **Full CRUD for Roles & Tactics:**
    *   **Goal:** Allow users to not only create but also **Edit** and **Delete** their custom roles and tactics.
    *   **Implementation:**
        *   Create "Edit Role" and "Edit Tactic" pages. They would start with a dropdown to select an existing custom definition.
        *   The form would be pre-populated with the current settings for that role/tactic.
        *   Add a "Delete" button with a confirmation step (`st.warning` and a second button) to prevent accidental deletion.
        *   Update `definitions_handler.py` with functions to modify or remove entries from the `definitions.json` file.

## 🔧 Technical & Code Refinements

Suggestions for refactoring and optimizing the existing codebase for better maintainability, performance, and flexibility.

*   **Consolidate Data Loading:**
    *   **Problem:** `load_data()` and `get_all_players()` are called in multiple page functions within `app.py`.
    *   **Solution:** Refactor `app.py`'s `main()` function to call these functions **once** at the beginning. Then, pass the resulting DataFrame and list of players as arguments to the page functions that need them. This leverages Streamlit's caching more effectively and simplifies the page functions.
    *   **Example:**
      ```python
      # In app.py
      def main():
          df = load_data() # Cached
          players = get_all_players() # Also cached
          page, uploaded_file = sidebar(df)

          if page == "Assign Roles":
              assign_roles_page(df) # Pass df as an argument
          elif page == "Player Comparison":
              player_comparison_page(players) # Pass players list
      ```

*   **Isolate Business Logic from UI:**
    *   **Problem:** Some functions, like `transfer_loan_management_page`, contain complex data calculation logic mixed directly with Streamlit UI code (`st.columns`, `st.button`, etc.).
    *   **Solution:** Move the data calculation part (e.g., the definitive identification of loan/sell candidates) into a dedicated function in `squad_logic.py`. The function in `app.py` then becomes much cleaner: it calls the logic function, receives a clean data structure (like a list of players), and then focuses only on displaying that data. This improves code readability and reusability.

*   **Centralize Definitions:**
    *   **Problem:** Key definitions like `GLOBAL_STAT_CATEGORIES` and `GK_STAT_CATEGORIES` are hardcoded in `constants.py`.
    *   **Solution:** Move these dictionaries into `config/definitions.json`. This fully centralizes all user-configurable game logic into one file. The `constants.py` file can then load them from there. This would allow an advanced user to re-categorize which attributes are "Important" vs. "Good" without ever touching the Python code.