# PACS Web Administration Frontend

React-based web administration interface for the PACS system.

## Features

Every page shows what the server reports, and says why when a request fails. Pages that could only
pretend have no controls: the server reads its settings at start, so Configuration shows them read-only.

- **Sign-in**: the server accepts OAuth 2.0 bearer tokens from one issuer and has no accounts of its own,
  so signing in means pasting an access token from that issuer. The token is kept for the tab only and
  sent as `Authorization: Bearer`; a refused or expired token returns to the sign-in page.
- **Dashboard**: system status and component health from `/system/status`, storage use, patient and
  study totals, association count and recent audit events. Nothing is shown that the server did not report.
- **Patients**: search by the start of a patient name (Korean names too), then studies, series and
  instances with their details.
- **Worklist**: create, change status (Scheduled, Started, Completed, the values the server keeps) and
  delete Modality Worklist items; DICOM modalities find them through C-FIND.
- **Audit Logs**: the stores, queries, retrieves, associations, REST API changes and refused requests
  the server recorded, filtered by event and outcome, exported as CSV or JSON. Administrators only.
- **Associations**: association totals and traffic since the DICOM server started.
- **Configuration**: the HTTP settings of the running server. Administrators only.

## Access

| Token scope | Allows |
|-------------|--------|
| `pacs.read` | Every page except Audit Logs and Configuration |
| `pacs.write` | That, plus worklist changes |
| `pacs.admin` | Everything, including Audit Logs, Configuration and deleting studies |

A valid token without any of these is refused at sign-in.

## Tech Stack

- **Framework**: React 19 with TypeScript
- **Build Tool**: Vite
- **Styling**: TailwindCSS v4
- **State Management**: React Query (TanStack Query)
- **Form Handling**: React Hook Form with Zod validation
- **Routing**: React Router v7
- **Icons**: Lucide React
- **Charts**: Recharts

## Prerequisites

- Node.js 20.19+ (Vite 8)
- npm 9+
- `pacs_server --http-port 8080 --http-rest-api` with its OAuth 2.0 options, or another `rest_server` host

## Getting Started

```bash
# Install dependencies
npm install

# Start development server
npm run dev

# Build for production
npm run build

# Preview production build
npm run preview

# Browser flows against a running server (see validation/browser/README.md)
npm run test:e2e
```

## Development

The development server runs on port 3000 and proxies API requests to `http://localhost:8080`, or to
`PACS_API_URL` when set. `npm run preview` uses the same proxy, so the app and the API share one origin
and a browser needs no CORS.

### Project Structure

```
web/
├── src/
│   ├── api/          # API client and types
│   ├── components/   # Reusable UI components
│   │   ├── layout/   # Layout components (Header, Sidebar)
│   │   ├── ui/       # Base UI components (Button, Card, Table)
│   │   └── worklist/ # Worklist-specific components (Form, Dialogs)
│   ├── hooks/        # Custom React hooks
│   ├── lib/          # Utility functions
│   ├── pages/        # Page components
│   └── types/        # TypeScript type definitions
├── public/           # Static assets
└── index.html        # Entry HTML file
```

### API Endpoints

The frontend connects to the following REST API endpoints, each needing a token with the scope shown:

| Endpoint | Scope | Description |
|----------|-------|-------------|
| `GET /api/v1/system/status` | read | System health status |
| `GET /api/v1/system/metrics` | read | Performance metrics, when the server has a provider |
| `GET /api/v1/system/config` | admin | HTTP server configuration |
| `GET /api/v1/system/version` | read | Server and API versions |
| `GET /api/v1/patients` | read | Patient list; `patient_name` ending in `*` matches by prefix |
| `GET /api/v1/patients/:id/studies` | read | Patient's studies |
| `GET /api/v1/studies` | read | Study list |
| `GET /api/v1/studies/:uid/series` | read | Study's series |
| `GET /api/v1/series/:uid/instances` | read | Series' instances |
| `GET /api/v1/worklist` | read | Worklist items (paginated) |
| `POST /api/v1/worklist` | write | Create worklist item |
| `PUT /api/v1/worklist/:id` | write | Change worklist item status |
| `DELETE /api/v1/worklist/:id` | write | Delete worklist item |
| `GET /api/v1/audit/logs` | admin | Audit log entries with filtering |
| `GET /api/v1/audit/export` | admin | Export audit logs (CSV/JSON) |
| `GET /api/v1/associations/active` | read | Association statistics |

## Building for Production

```bash
npm run build
```

The built files will be in the `dist/` directory. These can be served by any static file server or integrated with the C++ REST server.

## Configuration

### Vite Configuration

Edit `vite.config.ts` to customize:
- API proxy settings
- Build output directory
- Development server port

### TailwindCSS Theme

Edit `src/index.css` to customize the color theme and design tokens.

## License

BSD-3-Clause License - see [LICENSE](../LICENSE) for details.
