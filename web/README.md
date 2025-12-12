# PACS Web Administration Frontend

React-based web administration interface for the PACS system.

## Features

- **Dashboard**: Real-time system status with:
  - Active associations counter with live updates
  - Patient and study statistics
  - Storage usage pie charts (used/free and by modality)
  - System health component indicators
  - Recent activity feed from audit logs
  - Auto-refresh (10s for real-time data, 30s for status)
  - Manual refresh button
- **Patient/Study Browser**: Full DICOM hierarchy navigation with:
  - Patient search with debounced input (300ms)
  - Study list with date range and modality filters
  - Expandable Series/Instance navigation tree
  - Detailed metadata viewer panel for Study, Series, and Instance
  - Table and tree view options for study navigation
- **Worklist Management**: View and manage scheduled procedures
- **Audit Logs**: Comprehensive audit trail viewer with:
  - Searchable log table with server-side pagination
  - Filter by event type (Store/Query/Retrieve/Association/Login)
  - Filter by outcome (Success/Failure)
  - Date range picker for time-based filtering
  - Detail view modal with full event information
  - Export to CSV/JSON with applied filters
- **Configuration Management**: Comprehensive settings management with:
  - **REST API Settings**: Bind address, port, concurrency, timeout, body size limits
  - **DICOM Server Settings**: AE Title, port, max associations, PDU size, timeouts
  - **Storage Configuration**: Storage path, type (file/S3/Azure/HSM), compression
  - **Security Settings**: TLS/SSL toggle with certificate path configuration
  - **Logging Settings**: Log level, directory, rotation, audit trail options
  - Form validation with clear error messages
  - Toast notifications for save status feedback
  - Confirmation dialogs for critical changes
- **Associations**: Monitor active DICOM network connections

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

- Node.js 18+
- npm 9+
- PACS REST API server running on port 8080

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
```

## Development

The development server runs on port 3000 and proxies API requests to `http://localhost:8080`.

### Project Structure

```
web/
├── src/
│   ├── api/          # API client and types
│   ├── components/   # Reusable UI components
│   │   ├── layout/   # Layout components (Header, Sidebar)
│   │   └── ui/       # Base UI components (Button, Card, Table)
│   ├── hooks/        # Custom React hooks
│   ├── lib/          # Utility functions
│   ├── pages/        # Page components
│   └── types/        # TypeScript type definitions
├── public/           # Static assets
└── index.html        # Entry HTML file
```

### API Endpoints

The frontend connects to the following REST API endpoints:

| Endpoint | Description |
|----------|-------------|
| `GET /api/v1/system/status` | System health status |
| `GET /api/v1/system/metrics` | Performance metrics |
| `GET /api/v1/system/config` | REST server configuration |
| `PUT /api/v1/system/config` | Update REST server configuration |
| `GET /api/v1/system/dicom-config` | DICOM server configuration |
| `PUT /api/v1/system/dicom-config` | Update DICOM server configuration |
| `GET /api/v1/system/storage-config` | Storage configuration |
| `PUT /api/v1/system/storage-config` | Update storage configuration |
| `GET /api/v1/system/logging-config` | Logging configuration |
| `PUT /api/v1/system/logging-config` | Update logging configuration |
| `GET /api/v1/patients` | Patient list |
| `GET /api/v1/patients/:id` | Patient details |
| `GET /api/v1/patients/:id/studies` | Patient's studies |
| `GET /api/v1/studies/:uid/series` | Study's series |
| `GET /api/v1/series/:uid/instances` | Series' instances |
| `GET /api/v1/worklist` | Worklist items |
| `GET /api/v1/audit/logs` | Audit log entries with filtering |
| `GET /api/v1/audit/export` | Export audit logs (CSV/JSON) |
| `GET /api/v1/associations` | DICOM associations |

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

MIT License - see [LICENSE](../LICENSE) for details.
